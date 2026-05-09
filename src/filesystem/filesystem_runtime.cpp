#include "filesystem/filesystem_runtime.hpp"

namespace xash
{
namespace filesystem
{

FilesystemRuntime::FilesystemRuntime()
{
	reset();
}

void FilesystemRuntime::reset()
{
	m_state.reset();
}

void FilesystemRuntime::configure(const FilesystemStateConfig &config)
{
	m_state.configure(config);
}

FilesystemState &FilesystemRuntime::state()
{
	return m_state;
}

const FilesystemState &FilesystemRuntime::state() const
{
	return m_state;
}

searchpath_t *FilesystemRuntime::searchPaths() const
{
	return m_state.searchPaths();
}

void FilesystemRuntime::setSearchPaths(searchpath_t *value)
{
	m_state.setSearchPaths(value);
}

void FilesystemRuntime::prependSearchPath(searchpath_t *path,
	const SearchPathListOps &ops)
{
	if (!path || !ops.setNext)
		return;

	ops.setNext(ops.context, path, m_state.searchPaths());
	m_state.setSearchPaths(path);
}

searchpath_t *FilesystemRuntime::writePath() const
{
	return m_state.writePath();
}

void FilesystemRuntime::setWritePath(searchpath_t *value)
{
	m_state.setWritePath(value);
}

bool FilesystemRuntime::isWritePathMounted(const SearchPathListOps &ops) const
{
	searchpath_t *writePath = m_state.writePath();
	if (!writePath)
		return false;

	for (searchpath_t *path = m_state.searchPaths(); path;
		 path = ops.next ? ops.next(ops.context, path) : NULL)
	{
		if (path == writePath)
			return true;
	}

	return false;
}

SearchPathClearResult FilesystemRuntime::clearDynamicSearchPaths(
	const SearchPathListOps &ops)
{
	SearchPathClearResult result = {
		m_state.searchPaths(),
		m_state.writePath(),
		0,
		0
	};

	if (!ops.next || !ops.setNext)
		return result;

	searchpath_t *head = m_state.searchPaths();
	searchpath_t *previous = NULL;
	searchpath_t *current = head;

	while (current)
	{
		searchpath_t *next = ops.next(ops.context, current);

		if (ops.isStatic && ops.isStatic(ops.context, current))
		{
			previous = current;
			++result.keptCount;
			current = next;
			continue;
		}

		if (previous)
			ops.setNext(ops.context, previous, next);
		else
			head = next;

		if (ops.close)
			ops.close(ops.context, current);
		if (ops.freePath)
			ops.freePath(ops.context, current);

		++result.removedCount;
		current = next;
	}

	if (ops.markGamesNotAdded)
		ops.markGamesNotAdded(ops.context);

	m_state.setSearchPaths(head);
	if (!isWritePathMounted(ops))
		m_state.setWritePath(NULL);

	result.searchPaths = m_state.searchPaths();
	result.writePath = m_state.writePath();
	return result;
}

file_t *FilesystemRuntime::allocateFile(const FileHandleMemoryOps &ops,
	bool clear) const
{
	if (!ops.alloc)
		return NULL;

	return ops.alloc(ops.context, clear);
}

void FilesystemRuntime::freeFile(const FileHandleMemoryOps &ops,
	file_t *file) const
{
	if (file && ops.free)
		ops.free(ops.context, file);
}

searchpath_t *FilesystemRuntime::allocateSearchPath(
	const SearchPathMemoryOps &ops, bool clear) const
{
	if (!ops.alloc)
		return NULL;

	return ops.alloc(ops.context, clear);
}

void FilesystemRuntime::freeSearchPath(const SearchPathMemoryOps &ops,
	searchpath_t *path) const
{
	if (path && ops.free)
		ops.free(ops.context, path);
}

FilesystemRescanPlan FilesystemRuntime::beginRescan(uint32_t flags,
	const char *language, uint32_t allowedMountFlags,
	uint32_t localizationFlag)
{
	const uint32_t mountFlags = flags & allowedMountFlags;
	const bool localizationEnabled = (mountFlags & localizationFlag) != 0;

	m_state.setDirectPathsEnabled(false);
	m_state.setLanguage(localizationEnabled ? language : "");

	FilesystemRescanPlan plan = {
		mountFlags,
		localizationEnabled,
		m_state.language()
	};
	return plan;
}

}
}
