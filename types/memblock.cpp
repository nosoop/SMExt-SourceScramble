#include "extension.h"
#include "memblock.h"

#include <cstdlib>

HandleType_t g_MemoryBlockType = 0;

class MemoryBlock {
public:
	MemoryBlock(size_t size) {
		this->block = calloc(size, 1);
		this->size = size;
	}
	
	~MemoryBlock() {
		free(this->block);
	}
	
	size_t size;
	void* block;
};

void MemoryBlockHandler::OnHandleDestroy(HandleType_t type, void* object) {
	delete (MemoryBlock*) object;
}

HandleError ReadMemoryBlockHandle(Handle_t hndl, MemoryBlock **memoryBlock) {
	HandleSecurity sec;
	sec.pOwner = NULL;
	sec.pIdentity = myself->GetIdentity();
	
	return g_pHandleSys->ReadHandle(hndl, g_MemoryBlockType, &sec, (void **) memoryBlock);
}

/* static MemoryBlock.MemoryBlock(int size); */
cell_t sm_MemoryBlockCreate(IPluginContext *pContext, const cell_t *params) {
	size_t size = params[2];
	
	if (size <= 0) {
		return pContext->ThrowNativeError("Cannot allocate %d bytes of memory.", size);
	}
	
	MemoryBlock *pMemoryBlock = new MemoryBlock(size);
	
	if (!pMemoryBlock) {
		return 0;
	}
	
	if (!pMemoryBlock->block) {
		delete pMemoryBlock;
		return 0;
	}
	
	return g_pHandleSys->CreateHandle(g_MemoryBlockType, pMemoryBlock,
			pContext->GetIdentity(), myself->GetIdentity(), NULL);
}

cell_t sm_MemoryBlockPropAddressGet(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryBlock *pMemoryBlock;
	HandleError err;
	if ((err = ReadMemoryBlockHandle(hndl, &pMemoryBlock)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryBlock handle %x (error %d)", hndl, err);
	}
	
	return (uint32_t) pMemoryBlock->block;
}

cell_t sm_MemoryBlockPropSizeGet(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryBlock *pMemoryBlock;
	HandleError err;
	if ((err = ReadMemoryBlockHandle(hndl, &pMemoryBlock)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryBlock handle %x (error %d)", hndl, err);
	}
	
	return pMemoryBlock->size;
}
