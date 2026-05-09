#ifndef XASH_FILESYSTEM_FILESYSTEM_STATE_HPP
#define XASH_FILESYSTEM_FILESYSTEM_STATE_HPP

#include "xash3d_types.h"

typedef struct searchpath_s searchpath_t;
typedef struct file_s file_t;

namespace xash
{
namespace filesystem
{

struct FilesystemStateConfig
{
	const char *rootDir;
	const char *baseDir;
	const char *gameDir;
	const char *readOnlyDir;
	const char *language;
	searchpath_t *searchPaths;
	searchpath_t *writePath;
	bool directPathsEnabled;
};

class FilesystemState
{
public:
	FilesystemState();
	explicit FilesystemState(const FilesystemStateConfig &config);

	void reset();
	void configure(const FilesystemStateConfig &config);

	const char *rootDir() const;
	const char *baseDir() const;
	const char *gameDir() const;
	const char *readOnlyDir() const;
	const char *language() const;

	void setRootDir(const char *value);
	void setBaseDir(const char *value);
	void setGameDir(const char *value);
	void setReadOnlyDir(const char *value);
	void setLanguage(const char *value);

	searchpath_t *searchPaths() const;
	searchpath_t *writePath() const;
	void setSearchPaths(searchpath_t *value);
	void setWritePath(searchpath_t *value);

	bool directPathsEnabled() const;
	void setDirectPathsEnabled(bool enabled);

private:
	void copy(char *dst, size_t size, const char *value);

	char m_rootDir[MAX_SYSPATH];
	char m_baseDir[MAX_SYSPATH];
	char m_gameDir[MAX_SYSPATH];
	char m_readOnlyDir[MAX_SYSPATH];
	char m_language[MAX_STRING];
	searchpath_t *m_searchPaths;
	searchpath_t *m_writePath;
	bool m_directPathsEnabled;
};

}
}

#endif
