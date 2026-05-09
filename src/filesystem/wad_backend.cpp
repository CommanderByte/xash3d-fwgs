#include "filesystem/wad_backend.hpp"

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

}

WadBackend::WadBackend(const SearchPathMetadata &metadata)
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

WadBackend::WadBackend(const SearchPathMetadata &metadata, const WadBackendOps &ops)
	: m_metadata(metadata)
	, m_ops(ops)
	, m_hasOps(true)
{
}

const SearchPathMetadata &WadBackend::metadata() const
{
	return m_metadata;
}

void WadBackend::printInfo(char *dst, size_t size) const
{
	if (m_hasOps && m_ops.printInfo)
	{
		m_ops.printInfo(m_ops.context, dst, size);
		return;
	}

	CopyString(dst, size, m_metadata.source);
}

void WadBackend::close()
{
	if (m_hasOps && m_ops.close)
		m_ops.close(m_ops.context);
}

file_t *WadBackend::openFile(const char *path, const char *mode, int index)
{
	if (m_hasOps && m_ops.openFile)
		return m_ops.openFile(m_ops.context, path, mode, index);

	(void)path;
	(void)mode;
	(void)index;

	return NULL;
}

int WadBackend::fileTime(const char *path) const
{
	if (m_hasOps && m_ops.fileTime)
		return m_ops.fileTime(m_ops.context, path);

	(void)path;

	return -1;
}

int WadBackend::findFile(const char *path, char *fixedName, size_t len)
{
	if (m_hasOps && m_ops.findFile)
		return m_ops.findFile(m_ops.context, path, fixedName, len);

	(void)path;
	(void)fixedName;
	(void)len;

	return -1;
}

void WadBackend::search(stringlist_t *list, const char *pattern,
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

byte *WadBackend::loadFile(const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (m_hasOps && m_ops.loadFile)
		return m_ops.loadFile(m_ops.context, path, index, fileSize, alloc, freeFn);

	return ISearchPathBackend::loadFile(path, index, fileSize, alloc, freeFn);
}

}
}
