#pragma once

/**
 * A gameconf listener for the "MemPatches" section.
 */

#include "extension.h"

#include <am-string.h>
#include <am-vector.h>
#include <sm_stringhashmap.h>

using ByteVector = ke::Vector<uint8_t>;

class MemPatchGameConfig : public ITextListener_SMC {
public:
	SMCResult ReadSMC_NewSection(const SMCStates *states, const char *name);
	SMCResult ReadSMC_KeyValue(const SMCStates *states, const char *key, const char *value);
	SMCResult ReadSMC_LeavingSection(const SMCStates *states);
	void ReadSMC_ParseStart();
	
public:
	class MemoryPatchInfo {
	public:
		ke::AString signature;
		size_t offset;
		ByteVector vecPatch, vecVerify, vecPreserve;
	};
	
	enum ParseState {
		PState_None,
		PState_Root,
		PState_Runtime
	};
	
public:
	const MemoryPatchInfo* GetInfo(const char *key);
	
private:
	StringHashMap<MemoryPatchInfo*> m_MemPatchInfoMap;
};

extern MemPatchGameConfig g_MemPatchConfig;
