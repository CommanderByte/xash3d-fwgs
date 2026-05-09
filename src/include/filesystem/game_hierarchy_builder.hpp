#ifndef XASH_FILESYSTEM_GAME_HIERARCHY_BUILDER_HPP
#define XASH_FILESYSTEM_GAME_HIERARCHY_BUILDER_HPP

#include <stddef.h>
#include <stdint.h>

#include "xash3d_types.h"

namespace xash
{
namespace filesystem
{

enum class GameHierarchyMountKind
{
	ReadOnlyRoot,
	Downloads,
	GameDirectory,
	HighDefinition,
	Addon,
	LowViolence,
	Localization,
	Custom,
};

struct GameHierarchyBuildConfig
{
	const char *gameDirectory;
	const char *readOnlyDirectory;
	const char *language;
	uint32_t baseFlags;
	uint32_t readOnlyFlags;
	uint32_t optionalContentFlags;
	uint32_t gameCustomFlags;
	bool isGameDirectory;
	bool mountHighDefinition;
	bool mountAddon;
	bool mountLowViolence;
	bool mountLocalization;
};

struct GameHierarchyMountRequest
{
	GameHierarchyMountKind kind;
	char path[MAX_SYSPATH];
	uint32_t flags;
	bool enableDirectPaths;
};

class GameHierarchyBuilder
{
public:
	static const size_t MaxMountRequests = 8;

	GameHierarchyBuilder();

	void reset();
	bool build(const GameHierarchyBuildConfig &config);

	size_t count() const;
	const GameHierarchyMountRequest *requestAt(size_t index) const;
	const char *error() const;

	static const char *MountKindName(GameHierarchyMountKind kind);

private:
	bool addFormatted(
		GameHierarchyMountKind kind,
		uint32_t flags,
		bool enableDirectPaths,
		const char *format,
		const char *first,
		const char *second = nullptr);
	bool addRequest(
		GameHierarchyMountKind kind,
		const char *path,
		uint32_t flags,
		bool enableDirectPaths);
	void setError(const char *message);

	GameHierarchyMountRequest m_requests[MaxMountRequests];
	size_t m_count;
	const char *m_error;
};

}
}

#endif
