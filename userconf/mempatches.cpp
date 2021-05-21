#include "mempatches.h"
#include <cstdlib>
#include <cstring>

MemPatchGameConfig g_MemPatchConfig;

MemPatchGameConfig::ParseState g_ParseState;
unsigned int g_IgnoreLevel;

// The parent section type of a platform specific "windows" or "linux" section.
MemPatchGameConfig::ParseState g_PlatformOnlyState;

ke::AString g_CurrentSection;
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
		uint8_t byte = strtol(p, nullptr, 16);
		payload.append(byte);
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
	return !strcmp(name, "windows");
#elif defined _LINUX
	return !strcmp(name, "linux");
#elif defined _OSX
	return !strcmp(name, "mac");
#endif
}

/**
 * Return true if the name is for an operating system but not the current one.
 */
static inline bool IsNonTargetPlatformSection(const char* name) {
#if defined WIN32
	return (!strcmp(name, "linux") || !strcmp(name, "mac"));
#elif defined _LINUX
	return (!strcmp(name, "windows") || !strcmp(name, "mac"));
#elif defined _OSX
	return (!strcmp(name, "windows") || !strcmp(name, "linux"));
#endif
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
			smutils->LogError(myself, "Unreachable platform specific section in \"%s\" Function: line: %i col: %i", g_CurrentPatchInfo->signature.chars(), states->line, states->col);
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
			smutils->LogError(myself, "Unreachable platform specific section in \"%s\" Function: line: %i col: %i", g_CurrentPatchInfo->signature.chars(), states->line, states->col);
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
			smutils->LogError(myself, "Unexpected subsection \"%s\" (expected platform-specific)");
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
			g_CurrentPatchInfo->signature = value;
		} else if (!strcmp(key, "offset")) {
			// Support for IDA-style address offsets
			if (value[strlen(value)-1] == 'h') {
				g_CurrentPatchInfo->offset = (size_t) strtol(value, nullptr, 16);
			} else {
				g_CurrentPatchInfo->offset = atoi(value);
			}
		} else if (!strcmp(key, "patch")) {
			g_CurrentPatchInfo->vecPatch = ByteVectorFromString(value);
		} else if (!strcmp(key, "verify")) {
			g_CurrentPatchInfo->vecVerify = ByteVectorFromString(value);
		} else if (!strcmp(key, "preserve")) {
			g_CurrentPatchInfo->vecPreserve = ByteVectorFromString(value);
		}
		break;
	default:
		smutils->LogError(myself, "Unknown key in MemPatches section \"%s\": line: %i col: %i", key, states->line, states->col);
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
		m_MemPatchInfoMap.insert(g_CurrentSection.chars(), g_CurrentPatchInfo);
		
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