#include "mempatches.h"
#include <cstdlib>
#include <cstring>

MemPatchGameConfig g_MemPatchConfig;

MemPatchGameConfig::ParseState g_ParseState;
unsigned int g_IgnoreLevel;

// The parent section type of a platform specific "windows" or "linux" section.
MemPatchGameConfig::ParseState g_PlatformOnlyState;

std::string g_CurrentSection;
MemPatchGameConfig::MemoryPatchInfo *g_CurrentPatchInfo;

/**
 * Converts a byte string representation (in either signature or space-delimited hex format) to
 * a vector of bytes.
 */
ByteVector ByteVectorFromString(const char* s) {
	// TODO better parsing
	ByteVector payload;
	
	char* s1 = strdup(s);
	char* p = strtok(s1, "\\x ");
	while (p) {
		uint8_t byte = static_cast<uint8_t>(strtol(p, nullptr, 16));
		payload.push_back(byte);
		p = strtok(nullptr, "\\x ");
	}
	free(s1);
	
	return payload;
}

/**
 * Return true if the name is for the current operating system.
 */
static inline bool IsTargetPlatformSection(const char* name) {
#if defined WIN32
#if defined PLATFORM_64BITS
	return !strcmp(name, "windows64");
#else
	return !strcmp(name, "windows");
#endif // PLATFORM_64BITS

#elif defined _LINUX
#if defined PLATFORM_64BITS
	return !strcmp(name, "linux64");
#else
	return !strcmp(name, "linux");
#endif // PLATFORM_64BITS

#elif defined _OSX
	return !strcmp(name, "mac");
#endif
}

/**
 * Return true if the name is for an operating system but not the current one.
 */
static inline bool IsNonTargetPlatformSection(const char* name) {
	if (IsTargetPlatformSection(name)) {
		return false;
	}
	/* mutually exclusive with the above */
	return (
		!strcmp(name, "windows")
		|| !strcmp(name, "windows64")
		|| !strcmp(name, "linux")
		|| !strcmp(name, "linux64")
		|| !strcmp(name, "mac")
	);
}

/**
 * Game config "Functions" section parsing.
 */
SMCResult MemPatchGameConfig::ReadSMC_NewSection(const SMCStates *states, const char *name) {
	// Ignore child sections of ignored parent section
	if (g_IgnoreLevel > 0) {
		g_IgnoreLevel++;
		return SMCResult_Continue;
	}

	// Handle platform specific sections first.
	if (IsTargetPlatformSection(name)) {
		if (g_IgnoreLevel > 0) {
			smutils->LogError(myself, "Unreachable platform specific section in \"%s\" Function: line: %i col: %i", g_CurrentPatchInfo->signature.c_str(), states->line, states->col);
			return SMCResult_HaltFail;
		}
		
		// already nested into a section for our OS
		if (g_PlatformOnlyState != PState_None) {
			smutils->LogError(myself, "Duplicate platform specific section for \"%s\". Already parsing only for that OS: line: %i col: %i", name, states->line, states->col);
			return SMCResult_HaltFail;
		}
		g_PlatformOnlyState = g_ParseState;
		return SMCResult_Continue;
	} else if (IsNonTargetPlatformSection(name)) {
		if (g_PlatformOnlyState != PState_None) {
			smutils->LogError(myself, "Unreachable platform specific section in \"%s\" Function: line: %i col: %i", g_CurrentPatchInfo->signature.c_str(), states->line, states->col);
			return SMCResult_HaltFail;
		}
		g_IgnoreLevel++;
		return SMCResult_Continue;
	}

	switch (g_ParseState) {
		case PState_Root: {
			// fetch or create new struct, push new state
			auto sig = m_MemPatchInfoMap.find(name);
			if (sig.found()) {
				g_CurrentPatchInfo = sig->value;
			} else {
				g_CurrentPatchInfo = new MemoryPatchInfo();
			}
			g_CurrentSection = name;
			g_ParseState = PState_Runtime;
			break;
		}
		case PState_Runtime: {
			smutils->LogError(myself, "Unexpected subsection \"%s\" (expected platform-specific)", name);
			break;
		}
		default:
			smutils->LogError(myself, "Unknown subsection \"%s\": line: %i col: %i", name, states->line, states->col);
			return SMCResult_HaltFail;
	}

	return SMCResult_Continue;
}

SMCResult MemPatchGameConfig::ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value) {
	// We don't care for anything in this section or subsections.
	if (g_IgnoreLevel > 0) {
		return SMCResult_Continue;
	}

	switch (g_ParseState)
	{
	case PState_Runtime:
		if (!strcmp(key, "signature")) {
			if (value && value[0] != '\0') {
				g_CurrentPatchInfo->signature = value;
			} else {
				smutils->LogError(myself, "Error: empty signature in patch \"%s\" at line %i col %i", \
					g_CurrentSection.c_str(), states->line, states->col);
				return SMCResult_HaltFail;
			}
		} else if (!strcmp(key, "offset")) {
			// size_t - supports only unsigned int
			std::string s(value);
			char* end = nullptr;

			if (s.empty()) {
				smutils->LogError(myself, "Error: empty offset in patch \"%s\" at line %i col %i", \
									g_CurrentSection.c_str(), states->line, states->col);
				return SMCResult_HaltFail;
			} else {
				if (s.back() == 'h' || s.back() == 'H') {
					// IDA-style hex: "10h"
					s.pop_back();
					g_CurrentPatchInfo->offset = strtoul(s.c_str(), &end, 16);
				} else if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
					// C-style hex: "0x10"
					g_CurrentPatchInfo->offset = strtoul(s.c_str(), &end, 16);
				} else {
					// Decimal
					g_CurrentPatchInfo->offset = strtoul(s.c_str(), &end, 10);
				}

				if (*end != '\0') {
					smutils->LogError(myself, "Error: Invalid offset value \"%s\" at line %i col %i",\
										value, states->line, states->col);
					return SMCResult_HaltFail;
				}
			}
		} else if (!strcmp(key, "patch")) {
			g_CurrentPatchInfo->vecPatch = ByteVectorFromString(value);

			if (g_CurrentPatchInfo->vecPatch.size() < 1) {
				smutils->LogError(myself, "Error: empty patch section in patch \"%s\" at line %i col %i",
					g_CurrentSection.c_str(), states->line, states->col);
				return SMCResult_HaltFail;
			}
		} else if (!strcmp(key, "verify")) {
			g_CurrentPatchInfo->vecVerify = ByteVectorFromString(value);
			
			if (g_CurrentPatchInfo->vecVerify.size() < 1) {
				smutils->LogError(myself, "Error: empty verify section in patch \"%s\" at line %i col %i",
					g_CurrentSection.c_str(), states->line, states->col);
				return SMCResult_HaltFail;
			}
		} else if (!strcmp(key, "preserve")) {
			g_CurrentPatchInfo->vecPreserve = ByteVectorFromString(value);

			if (g_CurrentPatchInfo->vecPreserve.size() < 1) {
				smutils->LogError(myself, "Error: empty preserve section in patch \"%s\" at line %i col %i",
					g_CurrentSection.c_str(), states->line, states->col);
				return SMCResult_HaltFail;
			}
		} else {
			// Force users to check their gamedata in case something is incorrect.
			// It's possible that the user made a typo, and the extension doesn't display errors;
			// This has been verified in practice.
			smutils->LogError(myself, "Error: unknown key \"%s\" in patch \"%s\" at line %i col %i",
				key, g_CurrentSection.c_str(), states->line, states->col);
			return SMCResult_HaltFail;
		}
		break;
	default:
		smutils->LogError(myself, "Error: Unknown key in MemPatches section \"%s\": line: %i col: %i", key, states->line, states->col);
		return SMCResult_HaltFail;
	}

	return SMCResult_Continue;
}

SMCResult MemPatchGameConfig::ReadSMC_LeavingSection(const SMCStates *states) {
	// We were ignoring this section.  decrement and continue
	if (g_IgnoreLevel > 0) {
		g_IgnoreLevel--;
		return SMCResult_Continue;
	}

	// We were in a section only for our OS.
	if (g_PlatformOnlyState == g_ParseState) {
		g_PlatformOnlyState = PState_None;
		return SMCResult_Continue;
	}

	switch (g_ParseState)
	{
	case PState_Runtime:
		// pop section info
		g_ParseState = PState_Root;

		// save data for error messages
		g_CurrentPatchInfo->patchName = g_CurrentSection;

		m_MemPatchInfoMap.insert(g_CurrentSection.c_str(), g_CurrentPatchInfo);
		
		g_CurrentPatchInfo = nullptr;
		g_CurrentSection = "";
		break;
	}

	return SMCResult_Continue;
}

void MemPatchGameConfig::ReadSMC_ParseStart() {
	g_ParseState = PState_Root;
	g_IgnoreLevel = 0;
	g_PlatformOnlyState = PState_None;
}

const MemPatchGameConfig::MemoryPatchInfo* MemPatchGameConfig::GetInfo(const char *key) {
	auto sig = m_MemPatchInfoMap.find(key);
	if (sig.found()) {
		return sig->value;
	}
	return nullptr;
}