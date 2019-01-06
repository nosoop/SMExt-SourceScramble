#pragma once

class MemoryPatchHandler : public IHandleTypeDispatch {
	public:
	void OnHandleDestroy(HandleType_t type, void* object);
};

extern HandleType_t g_MemoryPatchType;

cell_t sm_MemoryPatchLoadFromConfig(IPluginContext *pContext, const cell_t *params);

cell_t sm_MemoryPatchValidate(IPluginContext *pContext, const cell_t *params);

cell_t sm_MemoryPatchEnable(IPluginContext *pContext, const cell_t *params);
cell_t sm_MemoryPatchDisable(IPluginContext *pContext, const cell_t *params);

cell_t sm_MemoryPatchPropAddressGet(IPluginContext *pContext, const cell_t *params);

const sp_nativeinfo_t g_MemoryPatchNatives[] = {
	{ "MemoryPatch.CreateFromConf", sm_MemoryPatchLoadFromConfig },
	
	{ "MemoryPatch.Validate", sm_MemoryPatchValidate },
	
	{ "MemoryPatch.Enable", sm_MemoryPatchEnable },
	{ "MemoryPatch.Disable", sm_MemoryPatchDisable },
	
	{ "MemoryPatch.Address.get", sm_MemoryPatchPropAddressGet },
	
	{NULL, NULL},
};
