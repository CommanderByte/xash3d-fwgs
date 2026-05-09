#include "filesystem/game_hierarchy_builder.hpp"

#include <stdio.h>
#include <string.h>

namespace xash
{
namespace filesystem
{

namespace
{

bool StringEmpty(const char *value)
{
	return value == nullptr || value[0] == '\0';
}

bool StartsWithAsciiAlpha(const char *value)
{
	if (StringEmpty(value))
		return false;

	const unsigned char ch = static_cast<unsigned char>(value[0]);
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

}

GameHierarchyBuilder::GameHierarchyBuilder()
	: m_count(0),
	  m_error(nullptr)
{
}

void GameHierarchyBuilder::reset()
{
	m_count = 0;
	m_error = nullptr;

	for (size_t i = 0; i < MaxMountRequests; ++i)
	{
		m_requests[i].kind = GameHierarchyMountKind::GameDirectory;
		m_requests[i].path[0] = '\0';
		m_requests[i].flags = 0;
		m_requests[i].enableDirectPaths = false;
	}
}

bool GameHierarchyBuilder::build(const GameHierarchyBuildConfig &config)
{
	reset();

	if (StringEmpty(config.gameDirectory))
		return true;

	if (!StringEmpty(config.readOnlyDirectory) &&
		!addFormatted(GameHierarchyMountKind::ReadOnlyRoot,
			config.readOnlyFlags,
			true,
			"%s/%s/",
			config.readOnlyDirectory,
			config.gameDirectory))
	{
		return false;
	}

	if (config.isGameDirectory &&
		!addFormatted(GameHierarchyMountKind::Downloads,
			config.customFlags,
			false,
			"%s_downloads/",
			config.gameDirectory))
	{
		return false;
	}

	if (!addFormatted(GameHierarchyMountKind::GameDirectory,
		config.baseFlags,
		false,
		"%s/",
		config.gameDirectory))
	{
		return false;
	}

	if (config.mountHighDefinition &&
		!addFormatted(GameHierarchyMountKind::HighDefinition,
			config.customFlags,
			false,
			"%s_hd/",
			config.gameDirectory))
	{
		return false;
	}

	if (config.mountAddon &&
		!addFormatted(GameHierarchyMountKind::Addon,
			config.customFlags,
			false,
			"%s_addon/",
			config.gameDirectory))
	{
		return false;
	}

	if (config.mountLowViolence &&
		!addFormatted(GameHierarchyMountKind::LowViolence,
			config.customFlags,
			false,
			"%s_lv/",
			config.gameDirectory))
	{
		return false;
	}

	if (config.mountLocalization &&
		StartsWithAsciiAlpha(config.language) &&
		!addFormatted(GameHierarchyMountKind::Localization,
			config.customFlags,
			false,
			"%s_%s/",
			config.gameDirectory,
			config.language))
	{
		return false;
	}

	if (config.isGameDirectory &&
		!addFormatted(GameHierarchyMountKind::Custom,
			config.customFlags,
			false,
			"%s/custom/",
			config.gameDirectory))
	{
		return false;
	}

	return true;
}

size_t GameHierarchyBuilder::count() const
{
	return m_count;
}

const GameHierarchyMountRequest *GameHierarchyBuilder::requestAt(size_t index) const
{
	if (index >= m_count)
		return nullptr;

	return &m_requests[index];
}

const char *GameHierarchyBuilder::error() const
{
	return m_error == nullptr ? "" : m_error;
}

const char *GameHierarchyBuilder::MountKindName(GameHierarchyMountKind kind)
{
	switch (kind)
	{
	case GameHierarchyMountKind::ReadOnlyRoot:
		return "readonly-root";
	case GameHierarchyMountKind::Downloads:
		return "downloads";
	case GameHierarchyMountKind::GameDirectory:
		return "game-directory";
	case GameHierarchyMountKind::HighDefinition:
		return "high-definition";
	case GameHierarchyMountKind::Addon:
		return "addon";
	case GameHierarchyMountKind::LowViolence:
		return "low-violence";
	case GameHierarchyMountKind::Localization:
		return "localization";
	case GameHierarchyMountKind::Custom:
		return "custom";
	default:
		return "unknown";
	}
}

bool GameHierarchyBuilder::addFormatted(
	GameHierarchyMountKind kind,
	uint32_t flags,
	bool enableDirectPaths,
	const char *format,
	const char *first,
	const char *second)
{
	char path[MAX_SYSPATH];
	const int written = second == nullptr ?
		snprintf(path, sizeof(path), format, first) :
		snprintf(path, sizeof(path), format, first, second);

	if (written < 0 || static_cast<size_t>(written) >= sizeof(path))
	{
		setError("mount request path was truncated");
		return false;
	}

	return addRequest(kind, path, flags, enableDirectPaths);
}

bool GameHierarchyBuilder::addRequest(
	GameHierarchyMountKind kind,
	const char *path,
	uint32_t flags,
	bool enableDirectPaths)
{
	if (m_count >= MaxMountRequests)
	{
		setError("mount request capacity exceeded");
		return false;
	}

	GameHierarchyMountRequest &request = m_requests[m_count];
	request.kind = kind;
	request.flags = flags;
	request.enableDirectPaths = enableDirectPaths;

	if (StringEmpty(path))
	{
		request.path[0] = '\0';
	}
	else
	{
		const int written = snprintf(request.path, sizeof(request.path), "%s", path);
		if (written < 0 || static_cast<size_t>(written) >= sizeof(request.path))
		{
			setError("mount request path was truncated");
			return false;
		}
	}

	++m_count;
	return true;
}

void GameHierarchyBuilder::setError(const char *message)
{
	m_error = message;
}

}
}
