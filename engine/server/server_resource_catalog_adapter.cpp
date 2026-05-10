#include "server_resource_catalog_adapter.h"

#include "engine/server/server_resource_catalog.hpp"

namespace
{

xash::engine::server::ResourceType ToModernResourceType(resourcetype_t type)
{
	using xash::engine::server::ResourceType;

	switch (type)
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

resourcetype_t ToLegacyResourceType(xash::engine::server::ResourceType type)
{
	using xash::engine::server::ResourceType;

	switch (type)
	{
	case ResourceType::Sound:
		return t_sound;
	case ResourceType::Skin:
		return t_skin;
	case ResourceType::Model:
		return t_model;
	case ResourceType::Decal:
		return t_decal;
	case ResourceType::Generic:
		return t_generic;
	case ResourceType::EventScript:
		return t_eventscript;
	case ResourceType::World:
		return t_world;
	case ResourceType::Unknown:
	default:
		return t_generic;
	}
}

sv_resource_catalog_entry_t ToLegacyEntry(
	const xash::engine::server::ResourceCatalogEntry &entry)
{
	sv_resource_catalog_entry_t legacy = {};
	legacy.should_add = entry.shouldAdd ? 1 : 0;
	legacy.type = ToLegacyResourceType(entry.resource.type);
	legacy.name = entry.resource.name;
	legacy.download_size = entry.resource.downloadSize;
	legacy.flags = static_cast<unsigned char>(entry.resource.flags);
	legacy.index = entry.resource.index;
	return legacy;
}

xash::engine::server::ResourceCatalogState ToModernState(
	const sv_resource_catalog_state_t *state)
{
	xash::engine::server::ResourceCatalogState modern = {};
	modern.soundSentenceMarkerAdded =
		state && state->sound_sentence_marker_added != 0;
	return modern;
}

void CopyModernState(
	sv_resource_catalog_state_t *legacy,
	const xash::engine::server::ResourceCatalogState &modern)
{
	if (!legacy)
		return;

	legacy->sound_sentence_marker_added = modern.soundSentenceMarkerAdded ? 1 : 0;
}

}

extern "C" void SV_ResourceCatalog_Init(sv_resource_catalog_state_t *state)
{
	if (!state)
		return;

	state->sound_sentence_marker_added = 0;
}

extern "C" int SV_ResourceCatalog_NeedsFileSize(resourcetype_t type, const char *name)
{
	return xash::engine::server::ResourceCatalogNeedsFileSize(
		ToModernResourceType(type),
		name) ? 1 : 0;
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddGeneric(
	const char *name,
	int index,
	int probed_download_size)
{
	return ToLegacyEntry(xash::engine::server::BuildGenericResourceCatalogEntry(
		name,
		index,
		probed_download_size));
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddSound(
	sv_resource_catalog_state_t *state,
	const char *name,
	int index,
	int probed_download_size)
{
	xash::engine::server::ResourceCatalogState modern = ToModernState(state);
	const xash::engine::server::ResourceCatalogEntry entry =
		xash::engine::server::BuildSoundResourceCatalogEntry(
			modern,
			name,
			index,
			probed_download_size);
	CopyModernState(state, modern);
	return ToLegacyEntry(entry);
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddModel(
	const char *name,
	int index,
	int probed_download_size,
	unsigned char flags)
{
	return ToLegacyEntry(xash::engine::server::BuildModelResourceCatalogEntry(
		name,
		index,
		probed_download_size,
		flags));
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddDecal(
	const char *name,
	int index)
{
	return ToLegacyEntry(xash::engine::server::BuildDecalResourceCatalogEntry(
		name,
		index));
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddEventScript(
	const char *name,
	int index,
	int probed_download_size)
{
	return ToLegacyEntry(xash::engine::server::BuildEventScriptResourceCatalogEntry(
		name,
		index,
		probed_download_size));
}
