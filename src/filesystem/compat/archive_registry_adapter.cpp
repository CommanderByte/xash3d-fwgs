#include "filesystem/compat/archive_registry_adapter.h"
#include "filesystem/compat/private/filesystem_private_types.h"

#include "filesystem/archive_registry.hpp"

namespace
{

using xash::filesystem::ArchiveBackendType;
using xash::filesystem::ArchiveFormatDescriptor;
using xash::filesystem::ArchiveRegistry;

ArchiveRegistry &DefaultArchiveRegistry()
{
	static ArchiveRegistry registry;
	static bool initialized = false;

	if (!initialized)
	{
		xash::filesystem::RegisterDefaultArchiveFormats(registry);
		initialized = true;
	}

	return registry;
}

int SearchPathTypeForBackend(ArchiveBackendType type)
{
	switch (type)
	{
	case ArchiveBackendType::Pak:
		return SEARCHPATH_PAK;
	case ArchiveBackendType::Wad:
		return SEARCHPATH_WAD;
	case ArchiveBackendType::Zip:
		return SEARCHPATH_ZIP;
	case ArchiveBackendType::Pk3Directory:
		return SEARCHPATH_PK3DIR;
	case ArchiveBackendType::Unknown:
	default:
		return -1;
	}
}

void FillRegistryEntry(
	fs_archive_registry_entry_t &entry,
	const ArchiveFormatDescriptor &descriptor)
{
	entry.extension = descriptor.extension;
	entry.searchpath_type = SearchPathTypeForBackend(descriptor.backendType);
	entry.real_archive = descriptor.realArchive ? 1 : 0;
	entry.auto_mount_contained_wads = descriptor.autoMountContainedWads ? 1 : 0;
	entry.scan_priority = descriptor.scanPriority;
	entry.debug_name = descriptor.debugName;
	entry.factory_name = descriptor.factoryName;
}

}

extern "C" size_t FS_ArchiveRegistry_Count( void )
{
	return DefaultArchiveRegistry().count();
}

extern "C" int FS_ArchiveRegistry_EntryAt( size_t index, fs_archive_registry_entry_t *entry )
{
	const ArchiveRegistry::Record *record = DefaultArchiveRegistry().recordAt(index);

	if (!record)
		return 0;

	if (entry)
		FillRegistryEntry(*entry, record->entry);

	return 1;
}

extern "C" int FS_ArchiveRegistry_Find(
	const char *extension,
	int only_real_archives,
	fs_archive_registry_entry_t *entry)
{
	const ArchiveFormatDescriptor *descriptor;

	if (!extension)
		return 0;

	descriptor = DefaultArchiveRegistry().find(extension);
	if (!descriptor)
		return 0;

	if (only_real_archives && !descriptor->realArchive)
		return 0;

	if (entry)
		FillRegistryEntry(*entry, *descriptor);

	return 1;
}
