#include "filesystem/filesystem_state.hpp"

namespace xash
{
namespace filesystem
{

FilesystemState::FilesystemState()
{
	reset();
}

FilesystemState::FilesystemState(const FilesystemStateConfig &config)
{
	reset();
	configure(config);
}

void FilesystemState::reset()
{
	m_rootDir[0] = '\0';
	m_baseDir[0] = '\0';
	m_gameDir[0] = '\0';
	m_readOnlyDir[0] = '\0';
	m_language[0] = '\0';
	m_searchPaths = NULL;
	m_writePath = NULL;
	m_directPathsEnabled = false;
}

void FilesystemState::configure(const FilesystemStateConfig &config)
{
	setRootDir(config.rootDir);
	setBaseDir(config.baseDir);
	setGameDir(config.gameDir);
	setReadOnlyDir(config.readOnlyDir);
	setLanguage(config.language);
	setSearchPaths(config.searchPaths);
	setWritePath(config.writePath);
	setDirectPathsEnabled(config.directPathsEnabled);
}

const char *FilesystemState::rootDir() const
{
	return m_rootDir;
}

const char *FilesystemState::baseDir() const
{
	return m_baseDir;
}

const char *FilesystemState::gameDir() const
{
	return m_gameDir;
}

const char *FilesystemState::readOnlyDir() const
{
	return m_readOnlyDir;
}

const char *FilesystemState::language() const
{
	return m_language;
}

void FilesystemState::setRootDir(const char *value)
{
	copy(m_rootDir, sizeof(m_rootDir), value);
}

void FilesystemState::setBaseDir(const char *value)
{
	copy(m_baseDir, sizeof(m_baseDir), value);
}

void FilesystemState::setGameDir(const char *value)
{
	copy(m_gameDir, sizeof(m_gameDir), value);
}

void FilesystemState::setReadOnlyDir(const char *value)
{
	copy(m_readOnlyDir, sizeof(m_readOnlyDir), value);
}

void FilesystemState::setLanguage(const char *value)
{
	copy(m_language, sizeof(m_language), value);
}

searchpath_t *FilesystemState::searchPaths() const
{
	return m_searchPaths;
}

searchpath_t *FilesystemState::writePath() const
{
	return m_writePath;
}

void FilesystemState::setSearchPaths(searchpath_t *value)
{
	m_searchPaths = value;
}

void FilesystemState::setWritePath(searchpath_t *value)
{
	m_writePath = value;
}

bool FilesystemState::directPathsEnabled() const
{
	return m_directPathsEnabled;
}

void FilesystemState::setDirectPathsEnabled(bool enabled)
{
	m_directPathsEnabled = enabled;
}

void FilesystemState::copy(char *dst, size_t size, const char *value)
{
	if (!dst || size == 0)
		return;

	if (!value)
		value = "";

	size_t i = 0;
	for (; i + 1 < size && value[i]; ++i)
		dst[i] = value[i];

	dst[i] = '\0';
}

}
}
