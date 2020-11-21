#pragma once

class MemoryBlockHandler : public IHandleTypeDispatch {
	public:
	void OnHandleDestroy(HandleType_t type, void* object);
	bool GetHandleApproxSize(HandleType_t type, void* object, unsigned int* size);
};

extern HandleType_t g_MemoryBlockType;

cell_t sm_MemoryBlockCreate(IPluginContext *pContext, const cell_t *params);

cell_t sm_MemoryBlockPropAddressGet(IPluginContext *pContext, const cell_t *params);
cell_t sm_MemoryBlockPropSizeGet(IPluginContext *pContext, const cell_t *params);
cell_t sm_MemoryBlockDisown(IPluginContext *pContext, const cell_t *params);

const sp_nativeinfo_t g_MemoryBlockNatives[] = {
	{ "MemoryBlock.MemoryBlock", sm_MemoryBlockCreate },
	
	{ "MemoryBlock.Address.get", sm_MemoryBlockPropAddressGet },
	{ "MemoryBlock.Size.get", sm_MemoryBlockPropSizeGet },
	{ "MemoryBlock.Disown", sm_MemoryBlockDisown },
	
	{NULL, NULL},
};
