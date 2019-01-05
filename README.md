# Source Scramble

A SourceMod extension that provides convenient methods to access platform-specific memory
patches stored in game configuration files.

## Origin

I just got really tired of writing plugin boilerplate for memory patching shenanigans.
[This is the most recent one I've written.][bot-collide]  The problems that stand out to me are:

1.  Hardcoded, platform-specific patch payload.
2.  No verification on bytes to be patched (what if a game update modifies the function?).
3.  Reverting the patch would have to be manually implemented.

[bot-collide]: https://gist.github.com/nosoop/08774339937179d0022a541b05b51c8c

SourceMod's built-in memory access functions are very powerful, but also very primitive.
Additionally, there is no method to access arbitrary sections of the game config file, so a
pure-SourcePawn implementation would force developers to either:

* Load and parse the config separate from `LoadGameConfigFile` with an `SMCParser` or similar,
or
* Have them place the required information in separate "Keys" section entries with particular
naming conventions.

Both solutions (which are the ones that I can think of) are fairly cumbersome.

The extension comes bundled with `sourcescramble_manager`, a helper plugin that reads gamedata
file names and patch names from a config file, then loads them all in.  For those simple patches
that don't need further configuration other than being toggled on.

## How to use

### Writing patches

This assumes you have a decent enough understanding of x86 assembly.

A new `MemPatches` section is added at the same level of `Addresses`, `Offsets`, and
`Signatures` in the game configuration file.  An example of a section is below:

```
"MemPatches"
{
	"CTraceFilterObject::ShouldHitEntity()::patch_tfbot_building_collisions"
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

* A subsection.  The name of the section will be the name used when referencing the patch.
* A function `signature` name referencing the name of a signature in a `Signatures` section
somewhere else in the game config file.
* The `offset` to patch.  Hexadecimal notation is supported with the `h` suffix, for easy
referencing in IDA or similar.
* `patch` (required) and `verify` (optional) Hex strings (`\x01\x02\x03` or `01 02 03` formats
supported) indicating the byte payload and a signature to match against at the previously
mentioned offset.

### Manually applying patches

This should be fairly self-explanatory:

```sourcepawn
// Handle hGameConf = LoadGameConfigFile(...);

// patches are cleaned up when the handle is deleted (which occurs when the owning plugin is unloaded)
MemoryPatch patch = MemoryPatch.CreateFromConf(hGameConf, "CTraceFilterObject::ShouldHitEntity()::patch_tfbot_building_collisions");

if (!patch.Verify()) {
	PrintToServer("[patchmem] Failed to verify patch.");
} else if (patch.Enable()) {
	PrintToServer("[patchmem] Enabled patch.");
}
```

## Future additions

One thing I was considering was allocation of arbitrary memory blocks.  Certain patches I've
done in the past use floating point operations that do not take immediates, so I've had to rely
on the replacement values being present elsewhere in the binary.
