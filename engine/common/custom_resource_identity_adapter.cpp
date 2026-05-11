/*
custom_resource_identity_adapter.cpp - private bridge to C++ resource identity helpers
*/

#include "custom_resource_identity_adapter.h"

#include "engine/server/resources/resource_identity.hpp"

#include <cstring>

namespace
{

xash::engine::server::ResourceType ToModernResourceType( resourcetype_t type )
{
	using xash::engine::server::ResourceType;

	switch( type )
	{
	case t_sound:
		return ResourceType::Sound;
	case t_skin:
		return ResourceType::Skin;
	case t_model:
		return ResourceType::Model;
	case t_decal:
		return ResourceType::Decal;
	case t_generic:
		return ResourceType::Generic;
	case t_eventscript:
		return ResourceType::EventScript;
	case t_world:
		return ResourceType::World;
	default:
		return ResourceType::Unknown;
	}
}

}

extern "C" int ResourceIdentityAdapter_SizeofResourceList( resource_t *pList, resourceinfo_t *ri )
{
	xash::engine::server::ResourceSizeSummary summary =
		xash::engine::server::EmptyResourceSizeSummary();

	if( ri )
		std::memset( ri, 0, sizeof( *ri ));

	if( !pList )
		return 0;

	for( resource_t *p = pList->pNext; p != pList && p; p = p->pNext )
	{
		xash::engine::server::ResourceDescriptor resource = {};
		resource.name = p->szFileName;
		resource.type = ToModernResourceType( p->type );
		resource.index = p->nIndex;
		resource.downloadSize = p->nDownloadSize;
		resource.flags = p->ucFlags;
		resource.md5Hash = p->rgucMD5_hash;

		xash::engine::server::AddResourceToSizeSummary( summary, resource );
	}

	if( ri )
	{
		for( int i = 0; i < xash::engine::server::kResourceSummaryBucketCount; ++i )
			ri->info[i].size = summary.sizeByType[i];
	}

	return summary.totalSize;
}
