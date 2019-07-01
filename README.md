# Source Scramble

A SourceMod extension that provides convenient methods to access platform-specific memory
patches stored in game configuration files.

## Origin

I just got really tired of writing plugin boilerplate for good, clean, memory patching
shenanigans.  I've written a number of them for public / private purposes, and they've been
generally a pain.

[This is the most recent one I've written.][bot-collide]  The problems that stand out to me are:

1.  Hardcoded, platform-specific patch payload.
2.  No verification on bytes to be patched (what if a game update modifies the function?).
3.  Reverting the patch would have to be manually implemented.

[bot-collide]: https://gist.github.com/nosoop/08774339937179d0022a541b05b51c8c

SourceMod's built-in memory access functions are very powerful, but also very primitive.
Additionally, there is no method to access arbitrary sections of the game config file, so a
pure-SourcePawn implementation would force developers to either:

* Load and parse the config separate from `LoadGameConfigFile` with an `SMCParser` or similar.
I've written this myself as a standalone include file, and there's still a decent amount of
manual management that needed doing for every plugin I wrote using it.
* Have them place the required information in separate "Keys" section entries with particular
naming conventions.  This is what [Memory Patcher][] and [No Thriller Taunt][] do.

[Memory Patcher]: https://forums.alliedmods.net/showthread.php?p=2617543
[No Thriller Taunt]: https://forums.alliedmods.net/showthread.php?t=171343

Both solutions (which are the ones that I can think of) are fairly cumbersome.

The extension comes bundled with `sourcescramble_manager`, a helper plugin that reads gamedata
file names and patch names from a config file, then loads them all in.  For those simple patches
that don't need further configuration other than being toggled on.

## Memory patching usage

### Writing patches

This assumes you have a decent enough understanding of x86 assembly.  I mean, why else would you
be writing one?

A new `MemPatches` section is added at the same level of `Addresses`, `Offsets`, and
`Signatures` in the game configuration file.  An example of a section is below:

```
"MemPatches"
{
	// this patch makes buildings solid to TFBots
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

* A subsection.  The name of the section will be the name used when getting a `MemoryPatch`
handle with `MemoryPatch.CreateFromConf()`.
* A function `signature` name referencing the name of a signature in a `Signatures` section
somewhere else in the game config file.
* The `offset` to patch.  Hexadecimal notation is supported with the `h` suffix, for easy
referencing in IDA or similar.
* `patch` (required) and `verify` (optional) Hex strings (`\x01\x02\x03` or `01 02 03` formats
supported) indicating the byte payload and a signature to match against at the previously
mentioned offset.

### Automatically applying patches

Add a key / value pair to `configs/sourcescramble_manager.cfg`, where the key and value
correspond to a gamedata file and patch name, respectively.

Alternatively, you can add your own config files to `configs/sourcescramble/`, making it easy
for end-users to install without having to edit the base config file.

Reload the plugin using `sm plugins reload sourcescramble_manager` to reload the patches.
No reload command is built-in, because the plugin intentionally leaks and doesn't keep track of
handles.  (The extension automatically disables patches when the handle is deleted, which
happens when the owning plugin is unloaded or reloaded.)

### Manually applying patches

For more complex cases (e.g., scoped hook memory patches or potentially dynamic patch
modifications), you'll have to write your own plugin.

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

patch.Disable();
```

## Memory blocks

A `MemoryBlock` is a `calloc`-allocated chunk of memory that can be accessed with
`StoreToAddress` and `LoadFromAddress` (indirectly via wrapped helper methods).

Some patches I've dealt with operate on fixed locations in memory (e.g., floating point load
operations that don't take immediate values), so with this I can point to the `MemoryBlock`
address space and put in whatever I need.
