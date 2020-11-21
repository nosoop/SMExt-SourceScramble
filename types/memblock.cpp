#include "extension.h"
#include "memblock.h"

#include <cstdlib>

HandleType_t g_MemoryBlockType = 0;

struct MemoryBlock {
	MemoryBlock(size_t size) : size{size}, disowned{false} {
		this->block = calloc(size, 1);
	}
	
	~MemoryBlock() {
		if (!disowned) {
			free(this->block);
		}
	}
	
	size_t size;
	bool disowned;
	void* block;
};

void MemoryBlockHandler::OnHandleDestroy(HandleType_t type, void* object) {
	delete (MemoryBlock*) object;
}

bool MemoryBlockHandler::GetHandleApproxSize(HandleType_t type, void* object, unsigned int* size) {
	*size = ((MemoryBlock*)object)->size;
	return true;
}

HandleError ReadMemoryBlockHandle(Handle_t hndl, MemoryBlock **memoryBlock) {
	HandleSecurity sec(NULL, myself->GetIdentity());
	return g_pHandleSys->ReadHandle(hndl, g_MemoryBlockType, &sec, (void **) memoryBlock);
}

/* static MemoryBlock.MemoryBlock(int size); */
cell_t sm_MemoryBlockCreate(IPluginContext *pContext, const cell_t *params) {
	cell_t size = params[1];
	
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
	
	return (uintptr_t) pMemoryBlock->block;
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

cell_t sm_MemoryBlockDisown(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryBlock *pMemoryBlock;
	HandleError err;
	if ((err = ReadMemoryBlockHandle(hndl, &pMemoryBlock)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryBlock handle %x (error %d)", hndl, err);
	}
	
	pMemoryBlock->disowned = true;
	return 0;
}
