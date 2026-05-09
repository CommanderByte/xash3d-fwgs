#include <stdlib.h>
#include <string.h>

#include "debugging/buffer_sink.hpp"
#include "filesystem/registry_snapshot.hpp"

using namespace xash::debugging;
using namespace xash::filesystem;
using namespace xash::filesystem::debugging;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SnapshotHeader TestHeader()
{
	SnapshotHeader header = {
		{ "xash3d.fs.registry", 1 },
		77,
		888
	};
	return header;
}

static bool TestCaptureDefaultArchiveRegistrySnapshot()
{
	ArchiveRegistry registry;
	FilesystemRegistryRecord records[4];
	FilesystemRegistrySnapshot snapshot;

	if (!RegisterDefaultArchiveFormats(registry).ok())
		return false;

	if (!CaptureArchiveRegistrySnapshot(registry, records,
		sizeof(records) / sizeof(records[0]), TestHeader(), snapshot).ok())
	{
		return false;
	}

	if (snapshot.recordCount != 4 || snapshot.records != records)
		return false;

	return records[0].order == 0 &&
		ExpectString(records[0].extension, "pak") &&
		records[0].backendType == ArchiveBackendType::Pak &&
		records[0].realArchive &&
		records[0].autoMountContainedWads &&
		records[0].scanPriority == 0 &&
		ExpectString(records[1].extension, "pk3") &&
		records[1].backendType == ArchiveBackendType::Zip &&
		records[2].backendType == ArchiveBackendType::Pk3Directory &&
		!records[2].realArchive &&
		records[3].backendType == ArchiveBackendType::Wad &&
		!records[3].autoMountContainedWads;
}

static bool TestCaptureRegistrySnapshotSmallStorage()
{
	ArchiveRegistry registry;
	FilesystemRegistryRecord records[1];
	FilesystemRegistrySnapshot snapshot;

	if (!RegisterDefaultArchiveFormats(registry).ok())
		return false;

	DebugStatus status = CaptureArchiveRegistrySnapshot(registry, records,
		sizeof(records) / sizeof(records[0]), TestHeader(), snapshot);

	return !status.ok() && status.code == DebugStatusCode::BufferTooSmall;
}

static bool TestHumanRegistrySnapshot()
{
	char output[768];
	FixedBufferDebugSink sink(output, sizeof(output));
	ArchiveRegistry registry;
	FilesystemRegistryRecord records[4];
	FilesystemRegistrySnapshot snapshot;

	if (!RegisterDefaultArchiveFormats(registry).ok())
		return false;

	if (!CaptureArchiveRegistrySnapshot(registry, records,
		sizeof(records) / sizeof(records[0]), TestHeader(), snapshot).ok())
	{
		return false;
	}

	if (!WriteHumanRegistrySnapshot(sink, snapshot).ok())
		return false;

	return ExpectString(sink.text(),
		"schema: xash3d.fs.registry.v1\n"
		"sequence: 77\n"
		"timestamp_usec: 888\n"
		"#  extension  backend  real  wads  priority  factory  name\n"
		"0  pak  pak  yes  yes  0  FS_AddPak_Fullpath  PAK archive\n"
		"1  pk3  zip  yes  yes  1  FS_AddZip_Fullpath  PK3 archive\n"
		"2  pk3dir  pk3dir  no  yes  2  FS_AddDir_Fullpath  PK3 directory\n"
		"3  wad  wad  yes  no  3  FS_AddWad_Fullpath  WAD archive\n");
}

static bool TestJsonRegistrySnapshot()
{
	char output[1536];
	FixedBufferDebugSink sink(output, sizeof(output));
	ArchiveRegistry registry;
	FilesystemRegistryRecord records[4];
	FilesystemRegistrySnapshot snapshot;

	if (!RegisterDefaultArchiveFormats(registry).ok())
		return false;

	if (!CaptureArchiveRegistrySnapshot(registry, records,
		sizeof(records) / sizeof(records[0]), TestHeader(), snapshot).ok())
	{
		return false;
	}

	if (!WriteJsonRegistrySnapshot(sink, snapshot).ok())
		return false;

	return ExpectString(sink.text(),
		"{\"schema\":\"xash3d.fs.registry\",\"version\":1,\"sequence\":77,"
		"\"timestampUsec\":888,\"archiveFormats\":["
		"{\"order\":0,\"extension\":\"pak\",\"backend\":\"pak\","
		"\"realArchive\":true,\"autoMountContainedWads\":true,"
		"\"scanPriority\":0,\"factory\":\"FS_AddPak_Fullpath\","
		"\"debugName\":\"PAK archive\"},"
		"{\"order\":1,\"extension\":\"pk3\",\"backend\":\"zip\","
		"\"realArchive\":true,\"autoMountContainedWads\":true,"
		"\"scanPriority\":1,\"factory\":\"FS_AddZip_Fullpath\","
		"\"debugName\":\"PK3 archive\"},"
		"{\"order\":2,\"extension\":\"pk3dir\",\"backend\":\"pk3dir\","
		"\"realArchive\":false,\"autoMountContainedWads\":true,"
		"\"scanPriority\":2,\"factory\":\"FS_AddDir_Fullpath\","
		"\"debugName\":\"PK3 directory\"},"
		"{\"order\":3,\"extension\":\"wad\",\"backend\":\"wad\","
		"\"realArchive\":true,\"autoMountContainedWads\":false,"
		"\"scanPriority\":3,\"factory\":\"FS_AddWad_Fullpath\","
		"\"debugName\":\"WAD archive\"}]}");
}

static bool TestInvalidRegistrySnapshot()
{
	char output[128];
	FixedBufferDebugSink sink(output, sizeof(output));
	FilesystemRegistrySnapshot snapshot = {
		{ { "xash3d.fs.registry", 1 }, 0, 0 },
		NULL,
		1,
	};
	DebugStatus status = WriteJsonRegistrySnapshot(sink, snapshot);

	return !status.ok() && status.code == DebugStatusCode::InvalidArgument;
}

int main()
{
	if (!TestCaptureDefaultArchiveRegistrySnapshot() ||
		!TestCaptureRegistrySnapshotSmallStorage() ||
		!TestHumanRegistrySnapshot() ||
		!TestJsonRegistrySnapshot() ||
		!TestInvalidRegistrySnapshot())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
