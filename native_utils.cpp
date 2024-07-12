#include "extension.h"

cell_t sm_CellRefToAddress(IPluginContext *pContext, const cell_t *params) {
#ifndef PLATFORM_64BITS
	cell_t* value;
	if (pContext->LocalToPhysAddr(params[1], &value) != SP_ERROR_NONE) {
		return pContext->ThrowNativeError("Failed to convert cell reference to address.");
	}
	return reinterpret_cast<intptr_t>(value);
#else
	return pContext->ThrowNativeError("GetAddressOfCell is not implemented for 64-bit platforms");
#endif

}

cell_t sm_CharArrayToAddress(IPluginContext *pContext, const cell_t *params) {
#ifndef PLATFORM_64BITS
	char* buffer;
	if (pContext->LocalToString(params[1], &buffer) != SP_ERROR_NONE) {
		return pContext->ThrowNativeError("Failed to convert string reference to address.");
	}
	return reinterpret_cast<intptr_t>(buffer);
#else
	return pContext->ThrowNativeError("GetAddressOfString is not implemented for 64-bit platforms");
#endif
}
