#include "filesystem/archive_registry.hpp"

namespace xash
{
namespace filesystem
{

namespace
{

static const ArchiveFormatDescriptor g_defaultArchiveFormats[] = {
	{
		"pak",
		ArchiveBackendType::Pak,
		true,
		true,
		0,
		"PAK archive",
		"FS_AddPak_Fullpath"
	},
	{
		"pk3",
		ArchiveBackendType::Zip,
		true,
		true,
		1,
		"PK3 archive",
		"FS_AddZip_Fullpath"
	},
	{
		"pk3dir",
		ArchiveBackendType::Pk3Directory,
		false,
		true,
		2,
		"PK3 directory",
		"FS_AddDir_Fullpath"
	},
	{
		"wad",
		ArchiveBackendType::Wad,
		true,
		false,
		3,
		"WAD archive",
		"FS_AddWad_Fullpath"
	},
};

}

const char *ArchiveBackendTypeName(ArchiveBackendType type)
{
	switch (type)
	{
	case ArchiveBackendType::Pak:
		return "pak";
	case ArchiveBackendType::Wad:
		return "wad";
	case ArchiveBackendType::Zip:
		return "zip";
	case ArchiveBackendType::Pk3Directory:
		return "pk3dir";
	case ArchiveBackendType::Unknown:
	default:
		return "unknown";
	}
}

const ArchiveFormatDescriptor *DefaultArchiveFormatDescriptors(size_t &count)
{
	count = sizeof(g_defaultArchiveFormats) / sizeof(g_defaultArchiveFormats[0]);
	return g_defaultArchiveFormats;
}

xash::utilities::RegistryStatus RegisterDefaultArchiveFormats(
	ArchiveRegistry &registry)
{
	size_t count = 0;
	const ArchiveFormatDescriptor *formats = DefaultArchiveFormatDescriptors(count);

	for (size_t i = 0; i < count; ++i)
	{
		xash::utilities::RegistryStatus status =
			registry.registerEntry(formats[i].extension, formats[i]);
		if (!status.ok())
			return status;
	}

	return xash::utilities::RegistryOk();
}

}
}
