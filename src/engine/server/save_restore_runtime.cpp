#include "engine/server/save_restore_runtime.hpp"

namespace xash
{
namespace engine
{
namespace server
{

SaveArchiveManifest ParseSaveArchiveManifest(
	const std::uint8_t *data,
	std::size_t size,
	std::size_t offset,
	int fileCount)
{
	SaveArchiveManifest manifest = {};

	if (fileCount < 0)
	{
		manifest.status = SaveRestoreParseStatus::InvalidCount;
		return manifest;
	}

	std::size_t cursor = offset;
	for (int i = 0; i < fileCount; ++i)
	{
		const SaveRestoreBundledFile bundled =
			ParseSaveRestoreBundledFile(data, size, cursor);
		if (bundled.status != SaveRestoreParseStatus::Ok)
		{
			manifest.status = bundled.status;
			manifest.consumedBytes = cursor - offset;
			return manifest;
		}

		SaveArchiveManifestEntry entry = {};
		entry.name = bundled.name;
		entry.size = bundled.size;
		entry.dataOffset = bundled.dataOffset;
		entry.nextOffset = bundled.nextOffset;
		manifest.entries.push_back(entry);
		cursor = bundled.nextOffset;
	}

	manifest.status = SaveRestoreParseStatus::Ok;
	manifest.consumedBytes = cursor - offset;
	return manifest;
}

SaveEntityPatchWritePlan BuildSaveEntityPatchWritePlan(
	const SaveEntityTableRowSnapshot *rows,
	std::size_t rowCount)
{
	SaveEntityPatchWritePlan plan = {};

	if (!rows)
		return plan;

	for (std::size_t i = 0; i < rowCount; ++i)
	{
		if (rows[i].removed)
			plan.removedEntityIndexes.push_back(static_cast<int>(i));
	}

	return plan;
}

SaveEntityPatchApplyPlan BuildSaveEntityPatchApplyPlan(
	const std::uint8_t *data,
	std::size_t size,
	int tableCount)
{
	const SaveRestoreEntityPatch patch =
		ParseSaveRestoreEntityPatch(data, size, tableCount);

	SaveEntityPatchApplyPlan plan = {};
	plan.status = patch.status;
	plan.markRemovedEntityIndexes = patch.removedEntityIndexes;
	plan.invalidEntityIndexes = patch.invalidEntityIndexes;
	return plan;
}

}
}
}
