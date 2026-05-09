#include "filesystem/android_assets_backend.hpp"

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

}

AndroidAssetsBackend::AndroidAssetsBackend(const SearchPathMetadata &metadata)
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
	m_ops.loadFile = NULL;
}

AndroidAssetsBackend::AndroidAssetsBackend(const SearchPathMetadata &metadata,
	const AndroidAssetsBackendOps &ops)
	: m_metadata(metadata)
	, m_ops(ops)
	, m_hasOps(true)
{
}

const SearchPathMetadata &AndroidAssetsBackend::metadata() const
{
	return m_metadata;
}

void AndroidAssetsBackend::printInfo(char *dst, size_t size) const
{
	if (m_hasOps && m_ops.printInfo)
	{
		m_ops.printInfo(m_ops.context, dst, size);
		return;
	}

	CopyString(dst, size, m_metadata.source);
}

void AndroidAssetsBackend::close()
{
	if (m_hasOps && m_ops.close)
		m_ops.close(m_ops.context);
}

file_t *AndroidAssetsBackend::openFile(const char *path, const char *mode,
	int index)
{
	if (m_hasOps && m_ops.openFile)
		return m_ops.openFile(m_ops.context, path, mode, index);

	(void)path;
	(void)mode;
	(void)index;

	return NULL;
}

int AndroidAssetsBackend::fileTime(const char *path) const
{
	if (m_hasOps && m_ops.fileTime)
		return m_ops.fileTime(m_ops.context, path);

	(void)path;

	return -1;
}

int AndroidAssetsBackend::findFile(const char *path, char *fixedName,
	size_t len)
{
	if (m_hasOps && m_ops.findFile)
		return m_ops.findFile(m_ops.context, path, fixedName, len);

	(void)path;
	(void)fixedName;
	(void)len;

	return -1;
}

void AndroidAssetsBackend::search(stringlist_t *list, const char *pattern,
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

byte *AndroidAssetsBackend::loadFile(const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (m_hasOps && m_ops.loadFile)
		return m_ops.loadFile(m_ops.context, path, index, fileSize, alloc,
			freeFn);

	return ISearchPathBackend::loadFile(path, index, fileSize, alloc, freeFn);
}

int FindAndroidAsset(const AndroidAssetsFindRuntime &runtime,
	const char *path, char *fixedName, size_t fixedNameSize)
{
	if (!runtime.openAsset || !runtime.closeAsset)
		return -1;

	void *asset = runtime.openAsset(runtime.context, path, 0);
	if (!asset)
		return -1;

	runtime.closeAsset(runtime.context, asset);

	if (fixedName)
		CopyString(fixedName, fixedNameSize, path);

	return 0;
}

void SearchAndroidAssets(const AndroidAssetsSearchRuntime &runtime,
	stringlist_t *list, const char *pattern, bool caseInsensitive)
{
	if (!runtime.alloc || !runtime.free || !runtime.listDirectory ||
		!runtime.listCreate || !runtime.listDestroy || !runtime.matchPattern ||
		!runtime.stringCount || !runtime.stringAt || !runtime.append)
	{
		return;
	}

	const char *separator = FindLastPathSeparator(pattern);
	const size_t basePathLength = separator ?
		static_cast<size_t>(separator + 1 - pattern) : 0;

	char *basePath = static_cast<char *>(runtime.alloc(runtime.context,
		basePathLength + 1, true));
	if (!basePath)
		return;

	if (basePathLength)
		memcpy(basePath, pattern, basePathLength);
	basePath[basePathLength] = '\0';

	stringlist_t *dirList = runtime.listCreate(runtime.context);
	if (!dirList)
	{
		runtime.free(runtime.context, basePath);
		return;
	}
	runtime.listDirectory(runtime.context, dirList, basePath);

	char temp[MAX_STRING];
	if (basePathLength >= sizeof(temp))
	{
		runtime.listDestroy(runtime.context, dirList);
		runtime.free(runtime.context, basePath);
		return;
	}
	CopyString(temp, sizeof(temp), basePath);

	const int dirCount = runtime.stringCount(runtime.context, dirList);
	for (int i = 0; i < dirCount; ++i)
	{
		CopyString(&temp[basePathLength], sizeof(temp) - basePathLength,
			runtime.stringAt(runtime.context, dirList, i));

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

	runtime.listDestroy(runtime.context, dirList);
	runtime.free(runtime.context, basePath);

	(void)caseInsensitive;
}

file_t *OpenAndroidAsset(const AndroidAssetsOpenRuntime &runtime,
	void *searchPath, const char *filename)
{
	if (!runtime.allocFile || !runtime.freeFile || !runtime.openAsset ||
		!runtime.openFileDescriptor || !runtime.closeAsset || !runtime.setupFile)
	{
		return NULL;
	}

	file_t *file = static_cast<file_t *>(runtime.allocFile(runtime.context));
	if (!file)
		return NULL;

	void *asset = runtime.openAsset(runtime.context, filename, 1);
	if (!asset)
	{
		runtime.freeFile(runtime.context, file);
		return NULL;
	}

	fs_offset_t offset = 0;
	fs_offset_t length = 0;
	const int handle = runtime.openFileDescriptor(runtime.context, asset,
		&offset, &length);

	runtime.setupFile(runtime.context, file, searchPath, handle, offset, length);
	runtime.closeAsset(runtime.context, asset);

	return file;
}

byte *LoadAndroidAsset(const AndroidAssetsLoadRuntime &runtime,
	const char *path, fs_offset_t *fileSize, void *(*alloc)(size_t),
	void (*freeFn)(void *))
{
	if (fileSize)
		*fileSize = 0;

	if (!runtime.openAsset || !runtime.length || !runtime.read ||
		!runtime.closeAsset || !alloc)
	{
		return NULL;
	}

	void *asset = runtime.openAsset(runtime.context, path, 2);
	if (!asset)
		return NULL;

	const fs_offset_t size = runtime.length(runtime.context, asset);
	byte *buffer = static_cast<byte *>(alloc(static_cast<size_t>(size + 1)));
	if (!buffer)
	{
		if (runtime.allocationFailed)
			runtime.allocationFailed(runtime.context,
				static_cast<size_t>(size + 1));
		runtime.closeAsset(runtime.context, asset);
		return NULL;
	}

	buffer[size] = '\0';

	if (runtime.read(runtime.context, asset, buffer,
		static_cast<size_t>(size)) < 0)
	{
		if (freeFn)
			freeFn(buffer);
		runtime.closeAsset(runtime.context, asset);
		return NULL;
	}

	runtime.closeAsset(runtime.context, asset);
	if (fileSize)
		*fileSize = size;

	return buffer;
}

}
}
