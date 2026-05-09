#ifndef XASH_FILESYSTEM_REGISTRY_SNAPSHOT_HPP
#define XASH_FILESYSTEM_REGISTRY_SNAPSHOT_HPP

#include <stddef.h>

#include "debugging/debug_sink.hpp"
#include "debugging/debug_types.hpp"
#include "filesystem/archive_registry.hpp"

namespace xash
{
namespace filesystem
{
namespace debugging
{

struct FilesystemRegistryRecord
{
	unsigned order;
	const char *extension;
	ArchiveBackendType backendType;
	bool realArchive;
	bool autoMountContainedWads;
	int scanPriority;
	const char *debugName;
	const char *factoryName;
};

struct FilesystemRegistrySnapshot
{
	xash::debugging::SnapshotHeader header;
	const FilesystemRegistryRecord *records;
	size_t recordCount;
};

xash::debugging::DebugStatus CaptureArchiveRegistrySnapshot(
	const ArchiveRegistry &registry,
	FilesystemRegistryRecord *records,
	size_t recordCapacity,
	const xash::debugging::SnapshotHeader &header,
	FilesystemRegistrySnapshot &out);

xash::debugging::DebugStatus WriteHumanRegistrySnapshot(
	xash::debugging::IDebugSink &sink,
	const FilesystemRegistrySnapshot &snapshot);

xash::debugging::DebugStatus WriteJsonRegistrySnapshot(
	xash::debugging::IDebugSink &sink,
	const FilesystemRegistrySnapshot &snapshot);

}
}
}

#endif
