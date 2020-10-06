#pragma once

cell_t sm_CellRefToAddress(IPluginContext *pContext, const cell_t *params);
cell_t sm_CharArrayToAddress(IPluginContext *pContext, const cell_t *params);

const sp_nativeinfo_t g_UtilNatives[] = {
	{ "GetAddressOfCell", sm_CellRefToAddress },
	{ "GetAddressOfString", sm_CharArrayToAddress },
	
	{NULL, NULL},
};
