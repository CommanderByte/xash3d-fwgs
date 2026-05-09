#include <stdlib.h>
#include <string.h>

#include "filesystem/game_hierarchy_builder.hpp"

using namespace xash::filesystem;

static bool ExpectRequest(
	const GameHierarchyBuilder &builder,
	size_t index,
	GameHierarchyMountKind kind,
	const char *path,
	uint32_t flags,
	bool enableDirectPaths)
{
	const GameHierarchyMountRequest *request = builder.requestAt(index);
	if (request == nullptr)
		return false;

	return request->kind == kind &&
		strcmp(request->path, path) == 0 &&
		request->flags == flags &&
		request->enableDirectPaths == enableDirectPaths;
}

static bool TestFullGameDirectoryOrder()
{
	GameHierarchyBuilder builder;
	GameHierarchyBuildConfig config = {};

	config.gameDirectory = "mod";
	config.readOnlyDirectory = "C:/Half-Life";
	config.language = "fr";
	config.baseFlags = 0x100;
	config.readOnlyFlags = 0x200;
	config.customFlags = 0x400;
	config.isGameDirectory = true;
	config.mountHighDefinition = true;
	config.mountAddon = true;
	config.mountLowViolence = true;
	config.mountLocalization = true;

	if (!builder.build(config) || builder.count() != 8)
		return false;

	return ExpectRequest(builder, 0, GameHierarchyMountKind::ReadOnlyRoot,
			"C:/Half-Life/mod/", 0x200, true) &&
		ExpectRequest(builder, 1, GameHierarchyMountKind::Downloads,
			"mod_downloads/", 0x400, false) &&
		ExpectRequest(builder, 2, GameHierarchyMountKind::GameDirectory,
			"mod/", 0x100, false) &&
		ExpectRequest(builder, 3, GameHierarchyMountKind::HighDefinition,
			"mod_hd/", 0x400, false) &&
		ExpectRequest(builder, 4, GameHierarchyMountKind::Addon,
			"mod_addon/", 0x400, false) &&
		ExpectRequest(builder, 5, GameHierarchyMountKind::LowViolence,
			"mod_lv/", 0x400, false) &&
		ExpectRequest(builder, 6, GameHierarchyMountKind::Localization,
			"mod_fr/", 0x400, false) &&
		ExpectRequest(builder, 7, GameHierarchyMountKind::Custom,
			"mod/custom/", 0x400, false);
}

static bool TestBaseDirectorySkipsGamedirOnlyMounts()
{
	GameHierarchyBuilder builder;
	GameHierarchyBuildConfig config = {};

	config.gameDirectory = "valve";
	config.language = "fr";
	config.baseFlags = 0x10;
	config.customFlags = 0x20;
	config.isGameDirectory = false;
	config.mountHighDefinition = true;
	config.mountLocalization = true;

	if (!builder.build(config) || builder.count() != 3)
		return false;

	return ExpectRequest(builder, 0, GameHierarchyMountKind::GameDirectory,
			"valve/", 0x10, false) &&
		ExpectRequest(builder, 1, GameHierarchyMountKind::HighDefinition,
			"valve_hd/", 0x20, false) &&
		ExpectRequest(builder, 2, GameHierarchyMountKind::Localization,
			"valve_fr/", 0x20, false);
}

static bool TestLocalizationRequiresAlphabeticLanguage()
{
	GameHierarchyBuilder builder;
	GameHierarchyBuildConfig config = {};

	config.gameDirectory = "mod";
	config.language = "1fr";
	config.baseFlags = 0x10;
	config.customFlags = 0x20;
	config.mountLocalization = true;

	if (!builder.build(config) || builder.count() != 1)
		return false;

	return ExpectRequest(builder, 0, GameHierarchyMountKind::GameDirectory,
		"mod/", 0x10, false);
}

static bool TestEmptyDirectoryBuildsNoRequests()
{
	GameHierarchyBuilder builder;
	GameHierarchyBuildConfig config = {};

	config.gameDirectory = "";

	return builder.build(config) && builder.count() == 0 &&
		builder.requestAt(0) == nullptr;
}

static bool TestMountKindNames()
{
	return strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::ReadOnlyRoot), "readonly-root") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::Downloads), "downloads") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::GameDirectory), "game-directory") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::HighDefinition), "high-definition") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::Addon), "addon") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::LowViolence), "low-violence") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::Localization), "localization") == 0 &&
		strcmp(GameHierarchyBuilder::MountKindName(
			GameHierarchyMountKind::Custom), "custom") == 0;
}

int main()
{
	if (!TestFullGameDirectoryOrder() ||
		!TestBaseDirectorySkipsGamedirOnlyMounts() ||
		!TestLocalizationRequiresAlphabeticLanguage() ||
		!TestEmptyDirectoryBuildsNoRequests() ||
		!TestMountKindNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
