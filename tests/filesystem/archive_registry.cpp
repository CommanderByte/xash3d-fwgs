#include <stdlib.h>
#include <string.h>

#include "filesystem/archive_registry.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static ArchiveFormatDescriptor MakePakDescriptor()
{
	ArchiveFormatDescriptor descriptor = {
		"pak",
		ArchiveBackendType::Pak,
		true,
		true,
		10,
		"PAK archive",
		"FS_AddPak_Fullpath"
	};
	return descriptor;
}

static bool TestArchiveBackendTypeNames()
{
	return ExpectString(ArchiveBackendTypeName(ArchiveBackendType::Pak), "pak") &&
		ExpectString(ArchiveBackendTypeName(ArchiveBackendType::Wad), "wad") &&
		ExpectString(ArchiveBackendTypeName(ArchiveBackendType::Zip), "zip") &&
		ExpectString(ArchiveBackendTypeName(ArchiveBackendType::Pk3Directory), "pk3dir") &&
		ExpectString(ArchiveBackendTypeName(ArchiveBackendType::Unknown), "unknown");
}

static bool TestArchiveRegistryDescriptorShape()
{
	ArchiveRegistry registry;
	ArchiveFormatDescriptor pak = MakePakDescriptor();

	if (!registry.registerEntry(pak.extension, pak).ok())
		return false;

	const ArchiveFormatDescriptor *found = registry.find("PAK");
	if (!found)
		return false;

	return found->backendType == ArchiveBackendType::Pak &&
		found->realArchive &&
		found->autoMountContainedWads &&
		found->scanPriority == 10 &&
		ExpectString(found->debugName, "PAK archive") &&
		ExpectString(found->factoryName, "FS_AddPak_Fullpath");
}

static bool TestDefaultArchiveDescriptors()
{
	size_t count = 0;
	const ArchiveFormatDescriptor *descriptors = DefaultArchiveFormatDescriptors(count);

	if (!descriptors || count != 4)
		return false;

	return ExpectString(descriptors[0].extension, "pak") &&
		descriptors[0].backendType == ArchiveBackendType::Pak &&
		descriptors[0].realArchive &&
		descriptors[0].autoMountContainedWads &&
		descriptors[0].scanPriority == 0 &&
		ExpectString(descriptors[1].extension, "pk3") &&
		descriptors[1].backendType == ArchiveBackendType::Zip &&
		descriptors[1].realArchive &&
		descriptors[1].autoMountContainedWads &&
		descriptors[1].scanPriority == 1 &&
		ExpectString(descriptors[2].extension, "pk3dir") &&
		descriptors[2].backendType == ArchiveBackendType::Pk3Directory &&
		!descriptors[2].realArchive &&
		descriptors[2].autoMountContainedWads &&
		descriptors[2].scanPriority == 2 &&
		ExpectString(descriptors[3].extension, "wad") &&
		descriptors[3].backendType == ArchiveBackendType::Wad &&
		descriptors[3].realArchive &&
		!descriptors[3].autoMountContainedWads &&
		descriptors[3].scanPriority == 3;
}

static bool TestRegisterDefaultArchiveFormats()
{
	ArchiveRegistry registry;

	if (!RegisterDefaultArchiveFormats(registry).ok())
		return false;

	if (registry.count() != 4)
		return false;

	const ArchiveRegistry::Record *record0 = registry.recordAt(0);
	const ArchiveRegistry::Record *record1 = registry.recordAt(1);
	const ArchiveRegistry::Record *record2 = registry.recordAt(2);
	const ArchiveRegistry::Record *record3 = registry.recordAt(3);

	if (!record0 || !record1 || !record2 || !record3)
		return false;

	if (!ExpectString(record0->key, "pak") ||
		!ExpectString(record1->key, "pk3") ||
		!ExpectString(record2->key, "pk3dir") ||
		!ExpectString(record3->key, "wad"))
	{
		return false;
	}

	const ArchiveFormatDescriptor *pk3 = registry.find("PK3");
	const ArchiveFormatDescriptor *pk3dir = registry.find("pk3dir");
	return pk3 && pk3->backendType == ArchiveBackendType::Zip &&
		pk3dir && pk3dir->backendType == ArchiveBackendType::Pk3Directory;
}

static bool TestRegisterDefaultArchiveFormatsRejectsDuplicates()
{
	ArchiveRegistry registry;

	if (!RegisterDefaultArchiveFormats(registry).ok())
		return false;

	xash::utilities::RegistryStatus duplicate = RegisterDefaultArchiveFormats(registry);
	return !duplicate.ok() &&
		duplicate.code == xash::utilities::RegistryStatusCode::DuplicateKey &&
		registry.count() == 4;
}

int main()
{
	if (!TestArchiveBackendTypeNames() ||
		!TestArchiveRegistryDescriptorShape() ||
		!TestDefaultArchiveDescriptors() ||
		!TestRegisterDefaultArchiveFormats() ||
		!TestRegisterDefaultArchiveFormatsRejectsDuplicates())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
