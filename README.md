# Source Scramble

[:coffee: fund my caffeine addiction :coffee:](https://buymeacoff.ee/nosoop)

A SourceMod extension that provides:
- An easy way for plugins to validate and patch platform-specific address locations with a game
configuration file.
- A new handle type for plugins to allocate / free their own memory blocks.
- Additional memory-related utilities.

The extension comes bundled with `sourcescramble_manager`, a helper plugin that reads gamedata
file names and patch names from a config file, then loads them all in.  For those simple patches
that don't need further configuration other than being toggled on.

## Installation

This is the installation process for end-users.

1.  Download and unpack the latest release (`package.zip` for Windows, `package.tar.gz` for
Linux) into your game directory.
2.  Install `sourcescramble_manager.smx` into your SourceMod installation's `plugins/`
directory.

There are two ways patches can be applied:

- managed patches, where `sourcescramble_manager.smx` handles installation / uninstallation in
a set-and-forget fashion; these are installed by adding a configuration entry in
`configs/sourcescramble/`
- unmanaged patches, where another plugin (usually by the patch author) performs the patching

It's up to the developer of each patch to provide instructions on how to install them.
Regardless, all patches do require a game configuration file installed in `gamedata/`.

## Origin

This was originally just dedicated to memory patching.  I had a number of gripes with existing
solutions like [Memory Patcher][], [No Thriller Taunt][], and one-off plugins for this purpose:

1.  Plugins either hardcode the solution in the plugin, or use some custom conventions in their
game config file (solutions I've seen / written myself use either pollute the `Keys` section or
do their own config parsing with `SMCParser`).
2.  There is no verification on bytes to be patched (what if a game update modifies the
function?).
3.  Reverting the patch would have to be manually implemented.  Some plugins neglect to do this.

Writing it as an extension allows it to:

1.  Leverage SourceMod's game configuration parsing logic.  No need to reparse the file, and
this provides a uniform convention for patches.
2.  Use the SourceMod handle system; this allows for managed cleanup and provides a relatively
nice API for developers to work with.

[Memory Patcher]: https://forums.alliedmods.net/showthread.php?p=2617543
[No Thriller Taunt]: https://forums.alliedmods.net/showthread.php?t=171343

## Developer usage

There are a number of things provided with this extension:

- Memory patches, which are a game config-dedicated section that allows a plugin to safely
overwrite and restore the contents of a memory location.
- Memory blocks, which provide a Handle type to allocate memory.
- Address-of natives.

Developing with this extension is intended for power users that are already getting their hands
dirty with server internals.  Most developers do not need this kind of flexibility.

### Memory patches

Memory patches allow developers to declare a byte payload to be written to memory.

#### Creating new patches

Since patches generally operate on machine code, you'll need to be familiar with that to write
patches.

A new `MemPatches` section is added at the same level of `Addresses`, `Offsets`, and
`Signatures` in the game configuration file.  An example of a section is below:

```
"MemPatches"
{
	// this patch makes buildings solid to TFBots by forcing certain code paths
	"CTraceFilterObject::ShouldHitEntity()::TFBotCollideWithBuildings"
	{
		"signature" "CTraceFilterObject::ShouldHitEntity()"
		"linux"
		{
			"offset"	"1A6h"
			"verify"	"\x75"
			"patch"		"\x70"
		}
		"windows"
		{
			"offset"	"9Ah"
			"verify"	"\x74"
			"patch"		"\x71"
		}
	}
}
```

A few things are present:

- A subsection.  The name of the section will be the name used when getting a `MemoryPatch`
handle with `MemoryPatch.CreateFromConf()`.
- A function `signature` name referencing the name of a signature in a `Signatures` section
somewhere else in the game config file.
- The `offset` to patch.  Hexadecimal notation is supported with the `h` suffix, for easy
referencing in IDA or similar.
- `patch` (required) and `verify` (optional) Hex strings (`\x01\x02\x03` or `01 02 03` formats
supported) indicating the byte payload and a signature to match against at the previously
mentioned offset.
	- `verify` signatures can use `\x2A` to indicate wildcards, same as SourceMod.
- An optional `preserve` hex string indicating which bits from the original location should be
copied to the patch.  (New in 0.7.x.)
	- For example, if you want to copy the high 4 bits in a byte from the original memory,
	that would be represented in binary as `0b11110000`, and you would use `\xF0`.

Any values written on top of an applied patch will be reverted back when the patch is removed.

#### Automatically applying patches

Once you've created a game configuration file that gets stored in `gamedata/`, you can apply it
in "managed" mode.

Create a new config file in `configs/sourcescramble/`, following the same format as
`configs/sourcescramble_manager.cfg`.  Key / value pairs should correspond to a gamedata file
and patch name, respectively.

Reload the plugin using `sm plugins reload sourcescramble_manager` to reload the patches.
No reload command is built-in, because the plugin intentionally leaks and doesn't keep track of
handles.  (The extension automatically disables patches when the handle is deleted, which
happens when the owning plugin is unloaded or reloaded.)

#### Manually applying patches

For more complex cases (e.g., scoped hook memory patches or potentially dynamic patch
modifications), you'll have to write your own plugin to patch / unpatch the memory as desired.

This should be fairly self-explanatory:

```sourcepawn
// Handle hGameConf = LoadGameConfigFile(...);

// as mentioned, patches are cleaned up when the handle is deleted
MemoryPatch patch = MemoryPatch.CreateFromConf(hGameConf, "CTraceFilterObject::ShouldHitEntity()::TFBotCollideWithBuildings");

if (!patch.Validate()) {
	ThrowError("Failed to verify patch.");
} else if (patch.Enable()) {
	LogMessage("Enabled patch.");
}

// ...

// restore the bytes that were in place when the patch was enabled
// any writes on top of the patched area are also wiped
patch.Disable();
```

### Memory blocks

A `MemoryBlock` is a `calloc`-allocated chunk of memory that can be accessed with
`StoreToAddress` and `LoadFromAddress` (indirectly via wrapped helper methods).

Some patches I've dealt with operate on fixed locations in memory (e.g., floating point load
operations that don't take immediate values), so with this I can point to the `MemoryBlock`
address space and put in whatever I need.

It's also a useful way of allocating structures for things like `SDKCall`s.

Basic use of the API:

```sourcepawn
// allocate and zero-initializes 4 bytes of memory
MemoryBlock block = new MemoryBlock(4);

block.StoreToOffset(0, view_as<int>(0.75), NumberType_Int32);

Address pFloatBlock = block.Address;

// frees the 4 bytes that was allocated
delete block;
```

### Address-of natives

New to Source Scramble 0.6.x, this allows a plugin to get the address of one of its own
variables (`cell_t` or `char[]`).

This replaces certain use cases of memory blocks; you can now point to an existing variable in
the plugin's memory space in cases when you need to read a float value or more granular control
in things like DHooks to send a fixed buffer.

```sourcepawn
float g_flValue;

Address pFloatLocation = GetAddressOfCell(g_flValue);
// patch an indirect load or whatever with the address of that float value

g_flValue = 0.75; // changes are reflected instantly wherever this memory location is referenced
```

Of course, this is all use-at-your-own-risk.
