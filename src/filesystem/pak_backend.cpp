#include "filesystem/pak_backend.hpp"

#include <ctype.h>
#include <stdio.h>
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

struct PakHeader
{
	int ident;
	int dirOffset;
	int dirLength;
};

const int kPakHeader = (('K' << 24) + ('C' << 16) + ('A' << 8) + 'P');
const int kMaxFilesInPack = 65536;

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

int SortPakCompare(const void *lhs, const void *rhs)
{
	const PakFileEntry *left = static_cast<const PakFileEntry *>(lhs);
	const PakFileEntry *right = static_cast<const PakFileEntry *>(rhs);

	return CaseInsensitiveCompare(left->name, right->name);
}

void StripLastPathElement(char *path)
{
	char *separator = path;

	for (char *current = path; *current; ++current)
	{
		if (*current == '/' || *current == '\\' || *current == ':')
			separator = current;
	}

	*separator = '\0';
}

}

PakBackend::PakBackend(const SearchPathMetadata &metadata)
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

PakBackend::PakBackend(const SearchPathMetadata &metadata, const PakBackendOps &ops)
	: m_metadata(metadata)
	, m_ops(ops)
	, m_hasOps(true)
{
}

const SearchPathMetadata &PakBackend::metadata() const
{
	return m_metadata;
}

void PakBackend::printInfo(char *dst, size_t size) const
{
	if (m_hasOps && m_ops.printInfo)
	{
		m_ops.printInfo(m_ops.context, dst, size);
		return;
	}

	CopyString(dst, size, m_metadata.source);
}

void PakBackend::close()
{
	if (m_hasOps && m_ops.close)
		m_ops.close(m_ops.context);
}

file_t *PakBackend::openFile(const char *path, const char *mode, int index)
{
	if (m_hasOps && m_ops.openFile)
		return m_ops.openFile(m_ops.context, path, mode, index);

	(void)path;
	(void)mode;
	(void)index;

	return NULL;
}

int PakBackend::fileTime(const char *path) const
{
	if (m_hasOps && m_ops.fileTime)
		return m_ops.fileTime(m_ops.context, path);

	(void)path;

	return -1;
}

int PakBackend::findFile(const char *path, char *fixedName, size_t len)
{
	if (m_hasOps && m_ops.findFile)
		return m_ops.findFile(m_ops.context, path, fixedName, len);

	(void)path;
	(void)fixedName;
	(void)len;

	return -1;
}

void PakBackend::search(stringlist_t *list, const char *pattern,
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

void SortPakEntries(PakFileEntry *files, int fileCount)
{
	if (!files || fileCount <= 1)
		return;

	qsort(files, static_cast<size_t>(fileCount), sizeof(files[0]), SortPakCompare);
}

int FindFileInPakArchive(const PakArchiveView &archive, const char *path,
	char *fixedName, size_t fixedNameSize)
{
	if (!archive.files || archive.fileCount <= 0)
		return -1;

	int left = 0;
	int right = archive.fileCount - 1;
	while (left <= right)
	{
		const int middle = (left + right) / 2;
		const int diff = CaseInsensitiveCompare(archive.files[middle].name, path);

		if (!diff)
		{
			if (fixedName)
				CopyString(fixedName, fixedNameSize, archive.files[middle].name);
			return middle;
		}

		if (diff > 0)
			right = middle - 1;
		else
			left = middle + 1;
	}

	return -1;
}

void SearchPakArchive(const PakArchiveView &archive,
	const PakSearchRuntime &runtime, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	if (!archive.files || archive.fileCount <= 0 || !runtime.matchPattern ||
		!runtime.stringCount || !runtime.stringAt || !runtime.append)
	{
		return;
	}

	for (int i = 0; i < archive.fileCount; ++i)
	{
		char temp[256];
		CopyString(temp, sizeof(temp), archive.files[i].name);

		while (temp[0])
		{
			if (runtime.matchPattern(runtime.context, temp, pattern, true))
			{
				int j = 0;
				const int count = runtime.stringCount(runtime.context, list);
				for (; j < count; ++j)
				{
					const char *existing =
						runtime.stringAt(runtime.context, list, j);
					if (existing && strcmp(existing, temp) == 0)
						break;
				}

				if (j == count)
					runtime.append(runtime.context, list, temp);
			}

			StripLastPathElement(temp);
		}
	}

	(void)caseInsensitive;
}

file_t *OpenPakEntry(const PakArchiveView &archive,
	const PakOpenFileRuntime &runtime, int index)
{
	if (!archive.files || !archive.handle || index < 0 ||
		index >= archive.fileCount || !runtime.openHandle)
	{
		return NULL;
	}

	const PakFileEntry &entry = archive.files[index];
	return runtime.openHandle(runtime.context, archive.handle,
		entry.filepos, entry.filelen);
}

PakLoadStatus OpenPakArchive(const PakOpenRuntime &runtime,
	const char *filename, PakOpenResult *result)
{
	if (!result)
		return PakLoadStatus::CouldNotOpen;

	result->handle = NULL;
	result->fileCount = 0;
	result->files = NULL;

	if (!runtime.openSystem || !runtime.close || !runtime.read ||
		!runtime.seek || !runtime.alloc || !runtime.free)
	{
		return PakLoadStatus::CouldNotOpen;
	}

	file_t *handle = runtime.openSystem(runtime.context, filename, "rb");
	if (!handle)
		return PakLoadStatus::CouldNotOpen;

	PakHeader header;
	if (runtime.read(runtime.context, handle, &header, sizeof(header)) !=
		static_cast<fs_offset_t>(sizeof(header)) ||
		header.ident != LittleLong(kPakHeader))
	{
		runtime.close(runtime.context, handle);
		return PakLoadStatus::BadHeader;
	}

	header.ident = LittleLong(header.ident);
	header.dirOffset = LittleLong(header.dirOffset);
	header.dirLength = LittleLong(header.dirLength);

	if (header.dirLength % static_cast<int>(sizeof(PakFileEntry)))
	{
		runtime.close(runtime.context, handle);
		return PakLoadStatus::BadFolders;
	}

	const int fileCount = header.dirLength / static_cast<int>(sizeof(PakFileEntry));
	if (fileCount > kMaxFilesInPack)
	{
		runtime.close(runtime.context, handle);
		return PakLoadStatus::TooManyFiles;
	}

	if (fileCount <= 0)
	{
		runtime.close(runtime.context, handle);
		return PakLoadStatus::NoFiles;
	}

	PakFileEntry *files = static_cast<PakFileEntry *>(
		runtime.alloc(runtime.context,
			static_cast<size_t>(fileCount) * sizeof(PakFileEntry), true));
	if (!files)
	{
		runtime.close(runtime.context, handle);
		return PakLoadStatus::Corrupted;
	}

	if (runtime.seek(runtime.context, handle, header.dirOffset, SEEK_SET) == -1 ||
		runtime.read(runtime.context, handle, files, header.dirLength) !=
			static_cast<fs_offset_t>(header.dirLength))
	{
		runtime.free(runtime.context, files);
		runtime.close(runtime.context, handle);
		return PakLoadStatus::Corrupted;
	}

	for (int i = 0; i < fileCount; ++i)
	{
		files[i].filepos = LittleLong(files[i].filepos);
		files[i].filelen = LittleLong(files[i].filelen);
	}

	SortPakEntries(files, fileCount);

	result->handle = handle;
	result->fileCount = fileCount;
	result->files = files;
	return PakLoadStatus::Ok;
}

}
}
