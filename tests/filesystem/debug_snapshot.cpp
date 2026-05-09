#include <stdlib.h>
#include <string.h>

#include "debugging/buffer_sink.hpp"
#include "filesystem/debug_snapshot.hpp"

using namespace xash::debugging;
using namespace xash::filesystem::debugging;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static FilesystemMountSnapshot TestSnapshot()
{
	static const FilesystemMountRecord records[] = {
		{
			0,
			FilesystemMountType::Directory,
			"valve/",
			1,
			"gamedir",
			true,
			false,
			"gamefolder",
			NULL,
		},
		{
			1,
			FilesystemMountType::Pak,
			"valve/pak0.pak",
			3,
			"gamedir,archive",
			false,
			true,
			"archive",
			NULL,
		},
	};
	FilesystemMountSnapshot snapshot = {
		{ { "xash3d.fs.mounts", 1 }, 9, 12345 },
		records,
		sizeof(records) / sizeof(records[0]),
	};
	return snapshot;
}

static bool TestMountTypeNames()
{
	return ExpectString(FilesystemMountTypeName(FilesystemMountType::Directory), "directory") &&
		ExpectString(FilesystemMountTypeName(FilesystemMountType::Pak), "pak") &&
		ExpectString(FilesystemMountTypeName(FilesystemMountType::Wad), "wad") &&
		ExpectString(FilesystemMountTypeName(FilesystemMountType::Zip), "zip") &&
		ExpectString(FilesystemMountTypeName(FilesystemMountType::Pk3Directory), "pk3dir") &&
		ExpectString(FilesystemMountTypeName(FilesystemMountType::AndroidAssets), "android_assets") &&
		ExpectString(FilesystemMountTypeName(FilesystemMountType::Unknown), "unknown");
}

static bool TestHumanMountSnapshot()
{
	char output[512];
	FixedBufferDebugSink sink(output, sizeof(output));

	if (!WriteHumanMountSnapshot(sink, TestSnapshot()).ok())
		return false;

	return ExpectString(sink.text(),
		"schema: xash3d.fs.mounts.v1\n"
		"sequence: 9\n"
		"timestamp_usec: 12345\n"
		"#  type            flags        write  archive  source\n"
		"0  directory  gamedir  yes     no      valve/  reason=gamefolder\n"
		"1  pak  gamedir,archive  no     yes      valve/pak0.pak  reason=archive\n");
}

static bool TestJsonMountSnapshot()
{
	char output[768];
	FixedBufferDebugSink sink(output, sizeof(output));

	if (!WriteJsonMountSnapshot(sink, TestSnapshot()).ok())
		return false;

	return ExpectString(sink.text(),
		"{\"schema\":\"xash3d.fs.mounts\",\"version\":1,\"sequence\":9,"
		"\"timestampUsec\":12345,\"searchPaths\":["
		"{\"order\":0,\"type\":\"directory\",\"source\":\"valve/\",\"flags\":1,"
		"\"flagsText\":\"gamedir\",\"writable\":true,\"archive\":false,"
		"\"mountReason\":\"gamefolder\",\"parentArchive\":null},"
		"{\"order\":1,\"type\":\"pak\",\"source\":\"valve/pak0.pak\",\"flags\":3,"
		"\"flagsText\":\"gamedir,archive\",\"writable\":false,\"archive\":true,"
		"\"mountReason\":\"archive\",\"parentArchive\":null}]}");
}

static bool TestJsonMountSnapshotOptionalStrings()
{
	char output[384];
	FixedBufferDebugSink sink(output, sizeof(output));
	FilesystemMountRecord record = {
		0,
		FilesystemMountType::Zip,
		"extras.pk3",
		0,
		NULL,
		false,
		true,
		NULL,
		"pak0.pak",
	};
	FilesystemMountSnapshot snapshot = {
		{ { "xash3d.fs.mounts", 1 }, 1, 2 },
		&record,
		1,
	};

	if (!WriteJsonMountSnapshot(sink, snapshot).ok())
		return false;

	return ExpectString(sink.text(),
		"{\"schema\":\"xash3d.fs.mounts\",\"version\":1,\"sequence\":1,"
		"\"timestampUsec\":2,\"searchPaths\":[{\"order\":0,\"type\":\"zip\","
		"\"source\":\"extras.pk3\",\"flags\":0,\"flagsText\":null,"
		"\"writable\":false,\"archive\":true,\"mountReason\":null,"
		"\"parentArchive\":\"pak0.pak\"}]}");
}

static bool TestInvalidMountSnapshot()
{
	char output[128];
	FixedBufferDebugSink sink(output, sizeof(output));
	FilesystemMountSnapshot missingRecords = {
		{ { "xash3d.fs.mounts", 1 }, 0, 0 },
		NULL,
		1,
	};

	DebugStatus missingRecordsStatus = WriteJsonMountSnapshot(sink, missingRecords);
	if (missingRecordsStatus.ok() ||
		missingRecordsStatus.code != DebugStatusCode::InvalidArgument)
	{
		return false;
	}

	FilesystemMountRecord record = {
		0,
		FilesystemMountType::Directory,
		NULL,
		0,
		NULL,
		false,
		false,
		NULL,
		NULL,
	};
	FilesystemMountSnapshot missingSource = {
		{ { "xash3d.fs.mounts", 1 }, 0, 0 },
		&record,
		1,
	};

	DebugStatus missingSourceStatus = WriteHumanMountSnapshot(sink, missingSource);
	return !missingSourceStatus.ok() &&
		missingSourceStatus.code == DebugStatusCode::InvalidArgument;
}

int main()
{
	if (!TestMountTypeNames() ||
		!TestHumanMountSnapshot() ||
		!TestJsonMountSnapshot() ||
		!TestJsonMountSnapshotOptionalStrings() ||
		!TestInvalidMountSnapshot())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
