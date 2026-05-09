#include "game_hierarchy_adapter.h"

#include "filesystem/game_hierarchy_builder.hpp"

using xash::filesystem::GameHierarchyBuildConfig;
using xash::filesystem::GameHierarchyBuilder;
using xash::filesystem::GameHierarchyMountRequest;

qboolean FS_AddGameHierarchyRequests(
	const char *dir,
	uint flags,
	const char *readOnlyDir,
	const char *language)
{
	const qboolean isGameDir = FBitSet(flags, FS_GAMEDIR_PATH);
	uint readOnlyFlags = FS_NOWRITE_PATH | (flags & (~(FS_GAMEDIR_PATH | FS_CUSTOM_PATH)));
	const uint optionalContentFlags = flags | FS_NOWRITE_PATH | FS_CUSTOM_PATH;
	const uint gameCustomFlags = FS_NOWRITE_PATH | FS_CUSTOM_PATH;
	GameHierarchyBuilder builder;
	GameHierarchyBuildConfig config = {};

	if (isGameDir)
		SetBits(readOnlyFlags, FS_GAMERODIR_PATH);

	config.gameDirectory = dir;
	config.readOnlyDirectory = readOnlyDir;
	config.language = language;
	config.baseFlags = flags;
	config.readOnlyFlags = readOnlyFlags;
	config.optionalContentFlags = optionalContentFlags;
	config.gameCustomFlags = gameCustomFlags;
	config.isGameDirectory = isGameDir != false;
	config.mountHighDefinition = FBitSet(flags, FS_MOUNT_HD) != false;
	config.mountAddon = FBitSet(flags, FS_MOUNT_ADDON) != false;
	config.mountLowViolence = FBitSet(flags, FS_MOUNT_LV) != false;
	config.mountLocalization = FBitSet(flags, FS_MOUNT_L10N) != false;

	if (!builder.build(config))
		return false;

	for (size_t i = 0; i < builder.count(); ++i)
	{
		const GameHierarchyMountRequest *request = builder.requestAt(i);
		if (request == nullptr)
			return false;

		if (request->enableDirectPaths)
			FS_AllowDirectPaths(true);

		FS_AddGameDirectory(request->path, request->flags);

		if (request->enableDirectPaths)
			FS_AllowDirectPaths(false);
	}

	return true;
}
