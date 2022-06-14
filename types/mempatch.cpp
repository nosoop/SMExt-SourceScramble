#include "extension.h"
#include "mempatch.h"

#include <sourcehook.h>
#include <sh_memory.h>

#include <am-vector.h>

#include "userconf/mempatches.h"

HandleType_t g_MemoryPatchType = 0;

void ByteVectorRead(ByteVector &vec, uint8_t* mem, size_t count) {
	vec.clear();
	for (size_t i = 0; i < count; i++) {
		vec.append(mem[i]);
	}
}

void ByteVectorWrite(ByteVector &vec, uint8_t* mem) {
	for (size_t i = 0; i < vec.length(); i++) {
		mem[i] = vec[i];
	}
}

class MemoryPatch {
public:
	MemoryPatch(uintptr_t pAddress, const MemPatchGameConfig::MemoryPatchInfo& info) {
		for (auto bit : info.vecPatch) {
			this->vecPatch.append(bit);
		}
		for (auto bit : info.vecVerify) {
			this->vecVerify.append(bit);
		}
		for (auto bit : info.vecPreserve) {
			this->vecPreserve.append(bit);
		}
		
		// ignore offset if address is bad
		this->pAddress = pAddress ? pAddress + (info.offset) : 0;
	}
	
	bool Enable() {
		if (vecRestore.length() > 0) {
			// already patched, disregard
			return false;
		}
		
		if (!this->Verify()) {
			return false;
		}
		ByteVectorRead(vecRestore, (uint8_t*) pAddress, vecPatch.length());
		
		SourceHook::SetMemAccess((void*) this->pAddress, vecPatch.length() * sizeof(uint8_t),
				SH_MEM_READ | SH_MEM_WRITE | SH_MEM_EXEC);
		ByteVectorWrite(vecPatch, (uint8_t*) pAddress);
		
		// 
		for (size_t i = 0; i < vecPatch.length(); i++) {
			uint8_t preserveBits = 0;
			if (i < vecPreserve.length()) {
				preserveBits = vecPreserve[i];
			}
			*((uint8_t*) pAddress + i) = (vecPatch[i] & ~preserveBits) | (vecRestore[i] & preserveBits);
		}
		
		return true;
	}
	
	void Disable() {
		if (vecRestore.length() == 0) {
			// no memory to restore, fug
			return;
		}
		ByteVectorWrite(vecRestore, (uint8_t*) pAddress);
		vecRestore.clear();
	}
	
	bool Verify() {
		if (!pAddress) {
			return false;
		}
		
		auto addr = (uint8_t*) pAddress;
		for (size_t i = 0; i < this->vecVerify.length(); i++) {
			if (vecVerify[i] != '*' && vecVerify[i] != addr[i]) {
				return false;
			}
		}
		return true;
	}
	
	~MemoryPatch() {
		this->Disable();
	}
	
	uintptr_t pAddress;
	ByteVector vecPatch, vecRestore, vecVerify, vecPreserve;
};

void MemoryPatchHandler::OnHandleDestroy(HandleType_t type, void* object) {
	delete (MemoryPatch*) object;
}

HandleError ReadMemoryPatchHandle(Handle_t hndl, MemoryPatch **memoryPatch) {
	HandleSecurity sec(NULL, myself->GetIdentity());
	return g_pHandleSys->ReadHandle(hndl, g_MemoryPatchType, &sec, (void **) memoryPatch);
}

/* static MemoryPatch FromGameConfig(Handle gamedata, const char[] name); */
cell_t sm_MemoryPatchLoadFromConfig(IPluginContext *pContext, const cell_t *params) {
	// TODO look up name in g_MemPatchCache, store reference to handle on success
	char *name;
	pContext->LocalToString(params[2], &name);
	
	const MemPatchGameConfig::MemoryPatchInfo* pInfo = g_MemPatchConfig.GetInfo(name);
	if (!pInfo) {
		return pContext->ThrowNativeError("Invalid patch name %s", name);
	}
	
	const auto& info = *pInfo;
	
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	HandleError err;
	IGameConfig *pConfig = gameconfs->ReadHandle(hndl, nullptr, &err);
	if (!pConfig) {
		return pContext->ThrowNativeError("Invalid game config handle %x (error %d)", hndl, err);
	}
	
	void* addr;
	if (!pConfig->GetMemSig(info.signature.chars(), &addr)) {
		return pContext->ThrowNativeError("Failed to locate signature for '%s' (mempatch '%s')", info.signature.chars(), name);
	}
	
	MemoryPatch *pMemoryPatch = new MemoryPatch((uintptr_t) addr, info);
	
	return g_pHandleSys->CreateHandle(g_MemoryPatchType, pMemoryPatch,
			pContext->GetIdentity(), myself->GetIdentity(), NULL);
}

cell_t sm_MemoryPatchValidate(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryPatch *pMemoryPatch;
	HandleError err;
	if ((err = ReadMemoryPatchHandle(hndl, &pMemoryPatch)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryPatch handle %x (error %d)", hndl, err);
	}
	
	return pMemoryPatch->Verify();
}

cell_t sm_MemoryPatchEnable(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryPatch *pMemoryPatch;
	HandleError err;
	if ((err = ReadMemoryPatchHandle(hndl, &pMemoryPatch)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryPatch handle %x (error %d)", hndl, err);
	}
	
	return pMemoryPatch->Enable();
}

cell_t sm_MemoryPatchDisable(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryPatch *pMemoryPatch;
	HandleError err;
	if ((err = ReadMemoryPatchHandle(hndl, &pMemoryPatch)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryPatch handle %x (error %d)", hndl, err);
	}
	
	pMemoryPatch->Disable();
	return true;
}

cell_t sm_MemoryPatchPropAddressGet(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryPatch *pMemoryPatch;
	HandleError err;
	if ((err = ReadMemoryPatchHandle(hndl, &pMemoryPatch)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryPatch handle %x (error %d)", hndl, err);
	}
	
	return pMemoryPatch->pAddress;
}