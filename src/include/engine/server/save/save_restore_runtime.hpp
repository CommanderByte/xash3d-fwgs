#ifndef XASH_ENGINE_SERVER_SAVE_RESTORE_RUNTIME_HPP
#define XASH_ENGINE_SERVER_SAVE_RESTORE_RUNTIME_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/server/save/save_restore_format.hpp"

namespace xash
{
namespace engine
{
namespace server
{

struct SaveArchiveManifestEntry
{
	std::string name;
	int size;
	std::size_t dataOffset;
	std::size_t nextOffset;
};

struct SaveArchiveManifest
{
	SaveRestoreParseStatus status;
	std::vector<SaveArchiveManifestEntry> entries;
	std::size_t consumedBytes;
};

struct SaveEntityTableRowSnapshot
{
	bool removed;
};

struct SaveEntityPatchWritePlan
{
	std::vector<int> removedEntityIndexes;
};

struct SaveEntityPatchApplyPlan
{
	SaveRestoreParseStatus status;
	std::vector<int> markRemovedEntityIndexes;
	std::vector<int> invalidEntityIndexes;
};

SaveArchiveManifest ParseSaveArchiveManifest(
	const std::uint8_t *data,
	std::size_t size,
	std::size_t offset,
	int fileCount);

SaveEntityPatchWritePlan BuildSaveEntityPatchWritePlan(
	const SaveEntityTableRowSnapshot *rows,
	std::size_t rowCount);

SaveEntityPatchApplyPlan BuildSaveEntityPatchApplyPlan(
	const std::uint8_t *data,
	std::size_t size,
	int tableCount);

}
}
}

#endif
