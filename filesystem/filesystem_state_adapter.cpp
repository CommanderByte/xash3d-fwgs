#include "filesystem_state_adapter.h"

#include "filesystem/filesystem_state.hpp"

namespace
{

xash::filesystem::FilesystemState g_filesystemState;

}

extern "C" {

void FS_FilesystemState_Reset(void)
{
	g_filesystemState.reset();
}

void FS_FilesystemState_Configure(const char *rootDir, const char *baseDir,
	const char *gameDir, const char *readOnlyDir, const char *language,
	searchpath_t *searchPaths, searchpath_t *writePath,
	qboolean directPathsEnabled)
{
	xash::filesystem::FilesystemStateConfig config = {
		rootDir,
		baseDir,
		gameDir,
		readOnlyDir,
		language,
		searchPaths,
		writePath,
		directPathsEnabled != false
	};
	g_filesystemState.configure(config);
}

const char *FS_FilesystemState_RootDir(void)
{
	return g_filesystemState.rootDir();
}

const char *FS_FilesystemState_BaseDir(void)
{
	return g_filesystemState.baseDir();
}

const char *FS_FilesystemState_GameDir(void)
{
	return g_filesystemState.gameDir();
}

const char *FS_FilesystemState_ReadOnlyDir(void)
{
	return g_filesystemState.readOnlyDir();
}

const char *FS_FilesystemState_Language(void)
{
	return g_filesystemState.language();
}

void FS_FilesystemState_SetRootDir(const char *value)
{
	g_filesystemState.setRootDir(value);
}

void FS_FilesystemState_SetBaseDir(const char *value)
{
	g_filesystemState.setBaseDir(value);
}

void FS_FilesystemState_SetGameDir(const char *value)
{
	g_filesystemState.setGameDir(value);
}

void FS_FilesystemState_SetReadOnlyDir(const char *value)
{
	g_filesystemState.setReadOnlyDir(value);
}

void FS_FilesystemState_SetLanguage(const char *value)
{
	g_filesystemState.setLanguage(value);
}

searchpath_t *FS_FilesystemState_SearchPaths(void)
{
	return g_filesystemState.searchPaths();
}

searchpath_t *FS_FilesystemState_WritePath(void)
{
	return g_filesystemState.writePath();
}

void FS_FilesystemState_SetSearchPaths(searchpath_t *value)
{
	g_filesystemState.setSearchPaths(value);
}

void FS_FilesystemState_SetWritePath(searchpath_t *value)
{
	g_filesystemState.setWritePath(value);
}

qboolean FS_FilesystemState_DirectPathsEnabled(void)
{
	return g_filesystemState.directPathsEnabled() ? true : false;
}

void FS_FilesystemState_SetDirectPathsEnabled(qboolean enabled)
{
	g_filesystemState.setDirectPathsEnabled(enabled != false);
}

}
