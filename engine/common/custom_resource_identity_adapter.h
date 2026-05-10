/*
custom_resource_identity_adapter.h - private bridge to C++ resource identity helpers
*/
#ifndef CUSTOM_RESOURCE_IDENTITY_ADAPTER_H
#define CUSTOM_RESOURCE_IDENTITY_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

int ResourceIdentityAdapter_SizeofResourceList( resource_t *pList, resourceinfo_t *ri );

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_RESOURCE_IDENTITY_ADAPTER_H
