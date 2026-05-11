#include "server_resource_catalog_adapter.h"

#include "engine/server/server_resource_catalog.hpp"
#include "resource_adapter_shared.hpp"

namespace
{

sv_resource_catalog_entry_t ToLegacyEntry(
	const xash::engine::server::ResourceCatalogEntry &entry)
{
	const xash::engine::server::adapter::LegacyResourceDescriptorFields fields =
		xash::engine::server::adapter::ToLegacyResourceDescriptorFields(
			entry.resource);
	sv_resource_catalog_entry_t legacy = {};
	legacy.should_add = entry.shouldAdd ? 1 : 0;
	legacy.type = fields.type;
	legacy.name = fields.name;
	legacy.download_size = fields.downloadSize;
	legacy.flags = fields.flags;
	legacy.index = fields.index;
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
		xash::engine::server::adapter::ToModernResourceType(type),
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
