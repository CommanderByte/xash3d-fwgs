#include "filesystem/directory_backend.hpp"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace xash
{
namespace filesystem
{

namespace
{

void CopyString(char *dst, size_t size, const char *text)
{
	if (!dst || size == 0)
		return;

	if (!text)
		text = "";

	size_t i = 0;
	for (; i + 1 < size && text[i]; ++i)
		dst[i] = text[i];

	dst[i] = '\0';
}

size_t CopyStringLength(char *dst, size_t size, const char *text)
{
	if (!text)
		text = "";

	const size_t length = strlen(text);
	CopyString(dst, size, text);
	return length;
}

void CopyStringSlice(char *dst, size_t size, const char *text, size_t length)
{
	if (!dst || size == 0)
		return;

	if (!text)
		text = "";

	size_t i = 0;
	for (; i + 1 < size && i < length && text[i]; ++i)
		dst[i] = text[i];

	dst[i] = '\0';
}

int CaseInsensitiveCompare(const char *lhs, const char *rhs)
{
	const unsigned char *left =
		reinterpret_cast<const unsigned char *>(lhs ? lhs : "");
	const unsigned char *right =
		reinterpret_cast<const unsigned char *>(rhs ? rhs : "");

	while (*left && *right)
	{
		const int leftChar = tolower(*left);
		const int rightChar = tolower(*right);

		if (leftChar != rightChar)
			return leftChar - rightChar;

		++left;
		++right;
	}

	return tolower(*left) - tolower(*right);
}

int SortDirectoryEntryCompare(const void *lhs, const void *rhs)
{
	const DirectoryEntry *left = static_cast<const DirectoryEntry *>(lhs);
	const DirectoryEntry *right = static_cast<const DirectoryEntry *>(rhs);

	return CaseInsensitiveCompare(left->name, right->name);
}

const char *FindCharOrEnd(const char *text, char value)
{
	if (!text)
		return "";

	while (*text && *text != value)
		++text;

	return text;
}

const char *FindLastPathSeparator(const char *text)
{
	const char *separator = NULL;

	for (const char *cursor = text ? text : ""; *cursor; ++cursor)
	{
		if (*cursor == '/' || *cursor == '\\' || *cursor == ':')
			separator = cursor;
	}

	return separator;
}

bool RuntimeHasDirectoryList(const DirectoryCaseRuntime &runtime)
{
	return runtime.listCreate && runtime.listDirectory && runtime.listDestroy &&
		runtime.stringCount && runtime.stringAt;
}

bool AppendToPath(char *dst, size_t *offset, size_t dstSize, const char *src,
	const DirectoryCaseRuntime &runtime, const char *fullPath,
	const char *operation)
{
	if (!dst || !offset || *offset >= dstSize)
	{
		if (runtime.overflow)
			runtime.overflow(runtime.context, fullPath, operation);
		return false;
	}

	const size_t copied = CopyStringLength(&dst[*offset], dstSize - *offset, src);
	*offset += copied;

	if (*offset >= dstSize)
	{
		if (runtime.overflow)
			runtime.overflow(runtime.context, fullPath, operation);
		return false;
	}

	return true;
}

void InitDirectoryEntries(DirectoryEntry *dir, stringlist_t *list,
	const DirectoryCaseRuntime &runtime)
{
	if (!dir || !RuntimeHasDirectoryList(runtime) || !runtime.alloc)
		return;

	const int count = runtime.stringCount(runtime.context, list);
	dir->entryCount = count;
	dir->entries = static_cast<DirectoryEntry *>(runtime.alloc(runtime.context,
		sizeof(DirectoryEntry) * static_cast<size_t>(count), false));

	if (!dir->entries)
	{
		dir->entryCount = DirectoryEntryEmpty;
		return;
	}

	for (int i = 0; i < count; ++i)
	{
		DirectoryEntry *entry = &dir->entries[i];
		CopyString(entry->name, sizeof(entry->name),
			runtime.stringAt(runtime.context, list, i));
		entry->entryCount = DirectoryEntryNotScanned;
		entry->entries = NULL;
		entry->backend = NULL;
	}

	qsort(dir->entries, static_cast<size_t>(dir->entryCount),
		sizeof(dir->entries[0]), SortDirectoryEntryCompare);
}

void MergeDirectoryEntries(DirectoryEntry *dir, stringlist_t *list,
	const DirectoryCaseRuntime &runtime)
{
	if (!dir || !dir->entries || !runtime.free)
		return;

	DirectoryEntry temp;
	temp.name[0] = '\0';
	temp.entryCount = DirectoryEntryNotScanned;
	temp.entries = NULL;
	temp.backend = NULL;
	InitDirectoryEntries(&temp, list, runtime);

	for (int i = 0; i < dir->entryCount; ++i)
	{
		DirectoryEntry *oldEntry = &dir->entries[i];
		if (oldEntry->entries == NULL)
			continue;

		const int index = FindDirectoryEntry(&temp, oldEntry->name);
		if (index < 0)
		{
			FreeDirectoryEntries(oldEntry, runtime);
			continue;
		}

		DirectoryEntry *newEntry = &temp.entries[index];
		newEntry->entryCount = oldEntry->entryCount;
		newEntry->entries = oldEntry->entries;
		oldEntry->entries = NULL;
	}

	runtime.free(runtime.context, dir->entries);
	dir->entryCount = temp.entryCount;
	dir->entries = temp.entries;
}

int MaybeUpdateDirectoryEntries(DirectoryEntry *dir, const char *path,
	const char *entryName, const DirectoryCaseRuntime &runtime)
{
	if (!dir || !RuntimeHasDirectoryList(runtime))
		return -1;

	stringlist_t *list = runtime.listCreate(runtime.context);
	if (!list)
		return -1;

	runtime.listDirectory(runtime.context, list, path, false);
	int result = -1;
	const int count = runtime.stringCount(runtime.context, list);

	if (count == 0)
	{
		FreeDirectoryEntries(dir, runtime);
		dir->entryCount = DirectoryEntryEmpty;
	}
	else if (dir->entryCount <= DirectoryEntryEmpty)
	{
		InitDirectoryEntries(dir, list, runtime);
		result = FindDirectoryEntry(dir, entryName);
	}
	else if (count != dir->entryCount)
	{
		MergeDirectoryEntries(dir, list, runtime);
		result = FindDirectoryEntry(dir, entryName);
	}
	else
	{
		int i = 0;
		for (; i < count; ++i)
		{
			if (CaseInsensitiveCompare(runtime.stringAt(runtime.context, list, i),
				entryName) == 0)
			{
				break;
			}
		}

		if (i != count)
		{
			MergeDirectoryEntries(dir, list, runtime);
			result = FindDirectoryEntry(dir, entryName);
		}
	}

	runtime.listDestroy(runtime.context, list);
	return result;
}

}

DirectoryBackend::DirectoryBackend(const SearchPathMetadata &metadata)
	: m_metadata(metadata)
	, m_hasOps(false)
{
	m_ops.context = NULL;
	m_ops.close = NULL;
	m_ops.printInfo = NULL;
	m_ops.openFile = NULL;
	m_ops.fileTime = NULL;
	m_ops.findFile = NULL;
	m_ops.search = NULL;
}

DirectoryBackend::DirectoryBackend(const SearchPathMetadata &metadata,
	const DirectoryBackendOps &ops)
	: m_metadata(metadata)
	, m_ops(ops)
	, m_hasOps(true)
{
}

const SearchPathMetadata &DirectoryBackend::metadata() const
{
	return m_metadata;
}

void DirectoryBackend::printInfo(char *dst, size_t size) const
{
	if (m_hasOps && m_ops.printInfo)
	{
		m_ops.printInfo(m_ops.context, dst, size);
		return;
	}

	CopyString(dst, size, m_metadata.source);
}

void DirectoryBackend::close()
{
	if (m_hasOps && m_ops.close)
		m_ops.close(m_ops.context);
}

file_t *DirectoryBackend::openFile(const char *path, const char *mode, int index)
{
	if (m_hasOps && m_ops.openFile)
		return m_ops.openFile(m_ops.context, path, mode, index);

	(void)path;
	(void)mode;
	(void)index;

	return NULL;
}

int DirectoryBackend::fileTime(const char *path) const
{
	if (m_hasOps && m_ops.fileTime)
		return m_ops.fileTime(m_ops.context, path);

	(void)path;

	return -1;
}

int DirectoryBackend::findFile(const char *path, char *fixedName, size_t len)
{
	if (m_hasOps && m_ops.findFile)
		return m_ops.findFile(m_ops.context, path, fixedName, len);

	(void)path;
	(void)fixedName;
	(void)len;

	return -1;
}

void DirectoryBackend::search(stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	if (m_hasOps && m_ops.search)
	{
		m_ops.search(m_ops.context, list, pattern, caseInsensitive);
		return;
	}

	(void)list;
	(void)pattern;
	(void)caseInsensitive;
}

void FreeDirectoryEntries(DirectoryEntry *dir,
	const DirectoryCaseRuntime &runtime)
{
	if (!dir)
		return;

	if (dir->entries)
	{
		for (int i = 0; i < dir->entryCount; ++i)
			FreeDirectoryEntries(&dir->entries[i], runtime);

		if (runtime.free)
			runtime.free(runtime.context, dir->entries);

		dir->entries = NULL;
	}

	dir->entryCount = DirectoryEntryNotScanned;
}

void PopulateDirectoryEntries(DirectoryEntry *dir, const char *path,
	const DirectoryCaseRuntime &runtime)
{
	if (!dir)
		return;

	if (!runtime.folderExists || !runtime.folderExists(runtime.context, path))
	{
		dir->entryCount = DirectoryEntryEmpty;
		dir->entries = NULL;
		return;
	}

	if (runtime.isDirectoryCaseSensitive &&
		!runtime.isDirectoryCaseSensitive(runtime.context, path))
	{
		dir->entryCount = DirectoryEntryCaseInsensitive;
		dir->entries = NULL;
		return;
	}

	if (!RuntimeHasDirectoryList(runtime))
	{
		dir->entryCount = DirectoryEntryEmpty;
		dir->entries = NULL;
		return;
	}

	stringlist_t *list = runtime.listCreate(runtime.context);
	if (!list)
	{
		dir->entryCount = DirectoryEntryEmpty;
		dir->entries = NULL;
		return;
	}

	runtime.listDirectory(runtime.context, list, path, false);
	if (runtime.stringCount(runtime.context, list) == 0)
	{
		dir->entryCount = DirectoryEntryEmpty;
		dir->entries = NULL;
	}
	else
	{
		InitDirectoryEntries(dir, list, runtime);
	}

	runtime.listDestroy(runtime.context, list);
}

int FindDirectoryEntry(DirectoryEntry *dir, const char *name)
{
	if (!dir || !dir->entries || dir->entryCount <= 0)
		return -1;

	int left = 0;
	int right = dir->entryCount - 1;
	while (left <= right)
	{
		const int middle = (left + right) / 2;
		const int diff = CaseInsensitiveCompare(dir->entries[middle].name, name);

		if (!diff)
			return middle;

		if (diff > 0)
			right = middle - 1;
		else
			left = middle + 1;
	}

	return -1;
}

bool FixDirectoryFileCase(DirectoryEntry *dir,
	const DirectoryCaseRuntime &runtime, const char *path, char *dst,
	size_t dstSize, bool createPath)
{
	if (!dir || !path)
		return false;

	size_t offset = 0;
	if (!AppendToPath(dst, &offset, dstSize, dir->name, runtime, path, "init"))
		return false;

	if (!path[0])
		return true;

	if (path[0] == '.' && path[1] == '.' &&
		(path[2] == '\0' || path[2] == '/'))
	{
		if (!AppendToPath(dst, &offset, dstSize, path, runtime, path,
			"escape to parent directory"))
		{
			return false;
		}

		return createPath || (runtime.fileOrFolderExists &&
			runtime.fileOrFolderExists(runtime.context, dst));
	}

	for (const char *prev = path, *next = FindCharOrEnd(prev, '/');;
		  prev = next + 1, next = FindCharOrEnd(prev, '/'))
	{
		bool upToDate = false;
		char entryName[MAX_SYSPATH];

		if (dir->entryCount == DirectoryEntryNotScanned)
		{
			PopulateDirectoryEntries(dir, dst, runtime);
			upToDate = true;
		}

		if (dir->entryCount == DirectoryEntryCaseInsensitive)
		{
			if (!AppendToPath(dst, &offset, dstSize, prev, runtime, path,
				"caseinsensitive entry"))
			{
				return false;
			}

			return createPath || (runtime.fileOrFolderExists &&
				runtime.fileOrFolderExists(runtime.context, dst));
		}

		CopyStringSlice(entryName, sizeof(entryName), prev,
			static_cast<size_t>(next - prev));

		int index = FindDirectoryEntry(dir, entryName);
		if (index < 0)
		{
			if (upToDate ||
				(index = MaybeUpdateDirectoryEntries(dir, dst, entryName,
					runtime)) < 0)
			{
				return createPath && AppendToPath(dst, &offset, dstSize, prev,
					runtime, path, "create path");
			}

			upToDate = true;
		}

		dir = &dir->entries[index];
		size_t tempOffset = offset;
		if (!AppendToPath(dst, &tempOffset, dstSize, dir->name, runtime, path,
			"case fix"))
		{
			return false;
		}

		if (!upToDate && runtime.fileOrFolderExists &&
			!runtime.fileOrFolderExists(runtime.context, dst))
		{
			dst[offset] = '\0';

			index = MaybeUpdateDirectoryEntries(dir, dst, entryName, runtime);
			if (index < 0)
			{
				return createPath && AppendToPath(dst, &offset, dstSize, prev,
					runtime, path, "create path rescan");
			}

			dir = &dir->entries[index];
			if (!AppendToPath(dst, &tempOffset, dstSize, dir->name, runtime,
				path, "case fix rescan"))
			{
				return false;
			}
		}

		offset = tempOffset;

		if (next[0] == '\0' || (next[0] == '/' && next[1] == '\0'))
			break;

		if (!AppendToPath(dst, &offset, dstSize, "/", runtime, path,
			"path separator"))
		{
			return false;
		}
	}

	return true;
}

int FindFileInDirectory(DirectoryEntry *dir,
	const DirectoryCaseRuntime &runtime, const char *searchPath,
	const char *path, char *fixedName, size_t fixedNameSize)
{
	char netPath[MAX_SYSPATH];

	if (!FixDirectoryFileCase(dir, runtime, path, netPath, sizeof(netPath),
		false))
	{
		return -1;
	}

	if (runtime.fileExists && runtime.fileExists(runtime.context, netPath))
	{
		if (fixedName)
		{
			const size_t prefixLength = searchPath ? strlen(searchPath) : 0;
			CopyString(fixedName, fixedNameSize, netPath + prefixLength);
		}
		return 0;
	}

	return -1;
}

void SearchDirectory(DirectoryEntry *dir, const DirectorySearchRuntime &runtime,
	stringlist_t *list, const char *pattern, bool caseInsensitive)
{
	if (!runtime.matchPattern || !runtime.stringCount || !runtime.stringAt ||
		!runtime.append || !runtime.caseRuntime.alloc ||
		!runtime.caseRuntime.free || !RuntimeHasDirectoryList(runtime.caseRuntime))
	{
		return;
	}

	const char *separator = FindLastPathSeparator(pattern);
	const size_t basePathLength = separator ?
		static_cast<size_t>(separator + 1 - pattern) : 0;

	char *basePath = static_cast<char *>(runtime.caseRuntime.alloc ?
		runtime.caseRuntime.alloc(runtime.caseRuntime.context,
			basePathLength + 1, true) : NULL);
	if (!basePath)
		return;

	if (basePathLength)
		memcpy(basePath, pattern, basePathLength);
	basePath[basePathLength] = '\0';

	char netPath[MAX_SYSPATH];
	if (!FixDirectoryFileCase(dir, runtime.caseRuntime, basePath, netPath,
		sizeof(netPath), false))
	{
		runtime.caseRuntime.free(runtime.caseRuntime.context, basePath);
		return;
	}

	stringlist_t *dirList = runtime.caseRuntime.listCreate(
		runtime.caseRuntime.context);
	if (!dirList)
	{
		runtime.caseRuntime.free(runtime.caseRuntime.context, basePath);
		return;
	}

	runtime.caseRuntime.listDirectory(runtime.caseRuntime.context, dirList,
		netPath, false);

	char temp[MAX_STRING];
	if (basePathLength >= sizeof(temp))
	{
		runtime.caseRuntime.listDestroy(runtime.caseRuntime.context, dirList);
		runtime.caseRuntime.free(runtime.caseRuntime.context, basePath);
		return;
	}
	CopyString(temp, sizeof(temp), basePath);

	const int dirCount = runtime.caseRuntime.stringCount(
		runtime.caseRuntime.context, dirList);
	for (int i = 0; i < dirCount; ++i)
	{
		CopyString(&temp[basePathLength], sizeof(temp) - basePathLength,
			runtime.caseRuntime.stringAt(runtime.caseRuntime.context, dirList, i));

		if (runtime.matchPattern(runtime.context, temp, pattern, true))
		{
			int resultIndex = 0;
			const int resultCount = runtime.stringCount(runtime.context, list);
			for (; resultIndex < resultCount; ++resultIndex)
			{
				const char *existing = runtime.stringAt(runtime.context, list,
					resultIndex);
				if (existing && strcmp(existing, temp) == 0)
					break;
			}

			if (resultIndex == resultCount)
				runtime.append(runtime.context, list, temp);
		}
	}

	runtime.caseRuntime.listDestroy(runtime.caseRuntime.context, dirList);
	runtime.caseRuntime.free(runtime.caseRuntime.context, basePath);

	(void)caseInsensitive;
}

file_t *OpenDirectoryFile(DirectoryEntry *dir,
	const DirectoryOpenRuntime &runtime, void *searchPath, const char *rootPath,
	const char *filename, const char *mode)
{
	(void)dir;

	if (!runtime.openSystem)
		return NULL;

	char path[MAX_SYSPATH];
	size_t offset = CopyStringLength(path, sizeof(path), rootPath);
	if (offset >= sizeof(path))
		return NULL;

	offset += CopyStringLength(&path[offset], sizeof(path) - offset, filename);
	if (offset >= sizeof(path))
		return NULL;

	file_t *file = runtime.openSystem(runtime.context, path, mode);
	if (!file)
		return NULL;

	if (runtime.setSearchPath)
		runtime.setSearchPath(runtime.context, file, searchPath);

	return file;
}

}
}
