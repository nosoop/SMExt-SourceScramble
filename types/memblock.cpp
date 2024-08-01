#include "extension.h"
#include "memblock.h"

#include <cstdlib>

#include <sourcehook.h>
#include <sh_memory.h>

HandleType_t g_MemoryBlockType = 0;

struct MemoryBlock {
	MemoryBlock(size_t size) : size{size}, disowned{false} {
		this->block = calloc(size, 1);
		
		SourceHook::SetMemAccess((void*) this->block, size * sizeof(uint8_t),
				SH_MEM_READ | SH_MEM_WRITE | SH_MEM_EXEC);
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

enum NumberType
{
	NumberType_Int8,
	NumberType_Int16,
	NumberType_Int32
};

bool GetNumberTypeSpan(cell_t numberType, size_t& span) {
	switch (numberType) {
		case 0: {
			span = 1;
			break;
		}
		case 1: {
			span = 2;
			break;
		}
		case 2: {
			span = 4;
			break;
		}
		default: {
			return false;
		}
	}
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
#ifndef PLATFORM_64BITS
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryBlock *pMemoryBlock;
	HandleError err;
	if ((err = ReadMemoryBlockHandle(hndl, &pMemoryBlock)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryBlock handle %x (error %d)", hndl, err);
	}
	
	return (uintptr_t) pMemoryBlock->block;
#else
	return pContext->ThrowNativeError("MemoryBlock.Address is not implemented for 64-bit platforms");
#endif
}

cell_t sm_MemoryBlockLoadCellFromOffset(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryBlock *pMemoryBlock;
	HandleError err;
	if ((err = ReadMemoryBlockHandle(hndl, &pMemoryBlock)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryBlock handle %x (error %d)", hndl, err);
	}
	
	size_t offset = params[2];
	size_t span;
	if (!GetNumberTypeSpan(params[3], span)) {
		return pContext->ThrowNativeError("Unknown NumberType %d.  Did the SourceMod developers add some new ones?", params[3]);
	} else if (offset < 0 || offset + span > pMemoryBlock->size) {
		return pContext->ThrowNativeError("Cannot perform MemoryBlock access of %d bytes at offset %d (limit %d)",
					span, offset, pMemoryBlock->size);
	}
	
	uintptr_t addr = reinterpret_cast<uintptr_t>(pMemoryBlock->block) + offset;
	switch(params[3]) {
		case NumberType_Int8:
			return *reinterpret_cast<uint8_t*>(addr);
		case NumberType_Int16:
			return *reinterpret_cast<uint16_t*>(addr);
		case NumberType_Int32:
			return *reinterpret_cast<uint32_t*>(addr);
	}
	// we should never hit this code path
	return pContext->ThrowNativeError("Unreachable error or failed to handle spans");
}

cell_t sm_MemoryBlockStoreCellToOffset(IPluginContext *pContext, const cell_t *params) {
	Handle_t hndl = static_cast<Handle_t>(params[1]);
	
	MemoryBlock *pMemoryBlock;
	HandleError err;
	if ((err = ReadMemoryBlockHandle(hndl, &pMemoryBlock)) != HandleError_None) {
		return pContext->ThrowNativeError("Invalid MemoryBlock handle %x (error %d)", hndl, err);
	}
	
	size_t offset = params[2];
	cell_t value = params[3];
	size_t span;
	if (!GetNumberTypeSpan(params[4], span)) {
		return pContext->ThrowNativeError("Unknown NumberType %d.  Did the SourceMod developers add some new ones?", params[3]);
	} else if (offset < 0 || offset + span > pMemoryBlock->size) {
		return pContext->ThrowNativeError("Cannot perform MemoryBlock access of %d bytes at offset %d (limit %d)",
					span, offset, pMemoryBlock->size);
	}
	
	uintptr_t addr = reinterpret_cast<uintptr_t>(pMemoryBlock->block) + offset;
	switch(params[3]) {
		case NumberType_Int8:
			*reinterpret_cast<int8_t*>(addr) = value;
			break;
		case NumberType_Int16:
			*reinterpret_cast<int16_t*>(addr) = value;
			break;
		case NumberType_Int32:
			*reinterpret_cast<int32_t*>(addr) = value;
			break;
	}
	return 0;
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
