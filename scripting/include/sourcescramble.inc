/**
 * Memory patching utilities.
 */

#if defined __sourcescramble_ext_included
	#endinput
#endif

#define __sourcescramble_ext_included

methodmap MemoryPatch < Handle {
	/**
	 * Loads a memory patch from a game config handle.
	 */
	public static native MemoryPatch CreateFromConf(Handle gameconf, const char[] name);
	
	/**
	 * Returns true if the contents at the memory location for the patch matches the contents
	 * specified in the game config.  If the game config does not specify any verify section,
	 * this always returns true.
	 */
	public native bool Validate();
	
	/**
	 * Enables the memory patch.  Returns false if the validation check fails or the patch has
	 * already been applied.
	 */
	public native bool Enable();
	
	/**
	 * Disables a memory patch.
	 */
	public native void Disable();
}

public Extension __ext_sourcescramble = {
	name = "Source Scramble",
	file = "sourcescramble.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public void __ext_sourcescramble_SetNTVOptional() {
}
#endif
