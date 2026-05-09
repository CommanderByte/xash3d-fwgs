#include "filesystem/zip_backend.hpp"

#include <ctype.h>
#include <stdint.h>
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

#pragma pack(push, 1)
struct ZipLocalHeader
{
	uint32_t signature;
	uint16_t version;
	uint16_t flags;
	uint16_t compressionFlags;
	uint32_t dosDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	uint16_t filenameLength;
	uint16_t extraFieldLength;
};

struct ZipCentralDirectoryHeader
{
	uint32_t signature;
	uint16_t version;
	uint16_t versionNeeded;
	uint16_t generalPurposeBitFlag;
	uint16_t flags;
	uint16_t modificationTime;
	uint16_t modificationDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	uint16_t filenameLength;
	uint16_t extraFieldLength;
	uint16_t fileCommentLength;
	uint16_t diskStart;
	uint16_t internalAttr;
	uint32_t externalAttr;
	uint32_t localHeaderOffset;
};

struct ZipEndOfCentralDirectory
{
	uint16_t diskNumber;
	uint16_t startDiskNumber;
	uint16_t numberCentralDirectoryRecord;
	uint16_t totalCentralDirectoryRecord;
	uint32_t sizeOfCentralDirectory;
	uint32_t centralDirectoryOffset;
	uint16_t commentLength;
};
#pragma pack(pop)

const uint32_t kZipHeaderLocalFile =
	(('K' << 8) + ('P') + (0x03 << 16) + (0x04 << 24));
const uint32_t kZipHeaderCentralDirectory =
	((0x02 << 24) + (0x01 << 16) + ('K' << 8) + 'P');
const uint32_t kZipHeaderEndOfCentralDirectory =
	((0x06 << 24) + (0x05 << 16) + ('K' << 8) + 'P');

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

int SortZipCompare(const void *lhs, const void *rhs)
{
	const ZipFileEntry *left = static_cast<const ZipFileEntry *>(lhs);
	const ZipFileEntry *right = static_cast<const ZipFileEntry *>(rhs);

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

bool ReadExact(const ZipOpenRuntime &runtime, file_t *file,
	void *buffer, size_t size)
{
	return runtime.read(runtime.context, file, buffer, size) ==
		static_cast<fs_offset_t>(size);
}

void ConvertCentralDirectoryHeader(ZipCentralDirectoryHeader *header)
{
	header->signature = LittleLong(header->signature);
	header->version = LittleShort(header->version);
	header->versionNeeded = LittleShort(header->versionNeeded);
	header->generalPurposeBitFlag = LittleShort(header->generalPurposeBitFlag);
	header->flags = LittleShort(header->flags);
	header->modificationTime = LittleShort(header->modificationTime);
	header->modificationDate = LittleShort(header->modificationDate);
	header->crc32 = LittleLong(header->crc32);
	header->compressedSize = LittleLong(header->compressedSize);
	header->uncompressedSize = LittleLong(header->uncompressedSize);
	header->filenameLength = LittleShort(header->filenameLength);
	header->extraFieldLength = LittleShort(header->extraFieldLength);
	header->fileCommentLength = LittleShort(header->fileCommentLength);
	header->diskStart = LittleShort(header->diskStart);
	header->internalAttr = LittleShort(header->internalAttr);
	header->externalAttr = LittleLong(header->externalAttr);
	header->localHeaderOffset = LittleLong(header->localHeaderOffset);
}

void ConvertLocalHeader(ZipLocalHeader *header)
{
	header->signature = LittleLong(header->signature);
	header->version = LittleShort(header->version);
	header->flags = LittleShort(header->flags);
	header->compressionFlags = LittleShort(header->compressionFlags);
	header->dosDate = LittleLong(header->dosDate);
	header->crc32 = LittleLong(header->crc32);
	header->compressedSize = LittleLong(header->compressedSize);
	header->uncompressedSize = LittleLong(header->uncompressedSize);
	header->filenameLength = LittleShort(header->filenameLength);
	header->extraFieldLength = LittleShort(header->extraFieldLength);
}

}

ZipBackend::ZipBackend(const SearchPathMetadata &metadata)
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

ZipBackend::ZipBackend(const SearchPathMetadata &metadata, const ZipBackendOps &ops)
	: m_metadata(metadata)
	, m_ops(ops)
	, m_hasOps(true)
{
}

const SearchPathMetadata &ZipBackend::metadata() const
{
	return m_metadata;
}

void ZipBackend::printInfo(char *dst, size_t size) const
{
	if (m_hasOps && m_ops.printInfo)
	{
		m_ops.printInfo(m_ops.context, dst, size);
		return;
	}

	CopyString(dst, size, m_metadata.source);
}

void ZipBackend::close()
{
	if (m_hasOps && m_ops.close)
		m_ops.close(m_ops.context);
}

file_t *ZipBackend::openFile(const char *path, const char *mode, int index)
{
	if (m_hasOps && m_ops.openFile)
		return m_ops.openFile(m_ops.context, path, mode, index);

	(void)path;
	(void)mode;
	(void)index;

	return NULL;
}

int ZipBackend::fileTime(const char *path) const
{
	if (m_hasOps && m_ops.fileTime)
		return m_ops.fileTime(m_ops.context, path);

	(void)path;

	return -1;
}

int ZipBackend::findFile(const char *path, char *fixedName, size_t len)
{
	if (m_hasOps && m_ops.findFile)
		return m_ops.findFile(m_ops.context, path, fixedName, len);

	(void)path;
	(void)fixedName;
	(void)len;

	return -1;
}

void ZipBackend::search(stringlist_t *list, const char *pattern,
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

byte *ZipBackend::loadFile(const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (m_hasOps && m_ops.loadFile)
		return m_ops.loadFile(m_ops.context, path, index, fileSize, alloc, freeFn);

	return ISearchPathBackend::loadFile(path, index, fileSize, alloc, freeFn);
}

void SortZipEntries(ZipFileEntry *files, int fileCount)
{
	if (!files || fileCount <= 1)
		return;

	qsort(files, static_cast<size_t>(fileCount), sizeof(files[0]), SortZipCompare);
}

int FindFileInZipArchive(const ZipArchiveView &archive, const char *path,
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

void SearchZipArchive(const ZipArchiveView &archive,
	const ZipSearchRuntime &runtime, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	if (!archive.files || archive.fileCount <= 0 || !runtime.matchPattern ||
		!runtime.stringCount || !runtime.stringAt || !runtime.append)
	{
		return;
	}

	for (int i = 0; i < archive.fileCount; ++i)
	{
		char temp[MAX_SYSPATH];
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

file_t *OpenZipEntry(const ZipArchiveView &archive,
	const ZipOpenFileRuntime &runtime, int index)
{
	if (!archive.files || !archive.handle || index < 0 ||
		index >= archive.fileCount || !runtime.openHandle)
	{
		return NULL;
	}

	const ZipFileEntry &entry = archive.files[index];
	file_t *file = runtime.openHandle(runtime.context, archive.handle,
		entry.offset, entry.size);

	if (!file)
		return NULL;

	if (entry.flags == static_cast<unsigned short>(ZipCompression::Deflated))
	{
		if (!runtime.setupDeflated ||
			!runtime.setupDeflated(runtime.context, file,
				entry.compressedSize, entry.name))
		{
			if (runtime.close)
				runtime.close(runtime.context, file);
			return NULL;
		}
	}
	else if (entry.flags != static_cast<unsigned short>(ZipCompression::Stored))
	{
		if (runtime.unsupportedCompression)
			runtime.unsupportedCompression(runtime.context, entry.name);
		if (runtime.close)
			runtime.close(runtime.context, file);
		return NULL;
	}

	return file;
}

byte *LoadZipEntry(const ZipArchiveView &archive,
	const ZipLoadFileRuntime &runtime, int index, fs_offset_t *fileSize,
	void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (fileSize)
		*fileSize = 0;

	if (!archive.files || !archive.handle || index < 0 ||
		index >= archive.fileCount || !runtime.seek || !runtime.read ||
		!alloc || !freeFn)
	{
		return NULL;
	}

	const ZipFileEntry &entry = archive.files[index];
	if (runtime.seek(runtime.context, archive.handle, entry.offset, SEEK_SET) == -1)
		return NULL;

	byte *output = static_cast<byte *>(alloc(static_cast<size_t>(entry.size) + 1));
	if (!output)
	{
		if (runtime.allocationFailed)
			runtime.allocationFailed(runtime.context,
				static_cast<size_t>(entry.size) + 1);
		return NULL;
	}
	output[entry.size] = '\0';

	if (entry.flags == static_cast<unsigned short>(ZipCompression::Stored))
	{
		const fs_offset_t readSize = runtime.read(
			runtime.context, archive.handle, output, static_cast<size_t>(entry.size));
		if (readSize != entry.size)
		{
			if (runtime.sizeMismatch)
				runtime.sizeMismatch(runtime.context, entry.name);
			freeFn(output);
			return NULL;
		}

		if (fileSize)
			*fileSize = entry.size;
		return output;
	}

	if (entry.flags == static_cast<unsigned short>(ZipCompression::Deflated))
	{
		if (!runtime.tempAlloc || !runtime.tempFree)
		{
			freeFn(output);
			return NULL;
		}

		byte *compressed = static_cast<byte *>(
			runtime.tempAlloc(runtime.context,
				static_cast<size_t>(entry.compressedSize) + 1));
		if (!compressed)
		{
			if (runtime.allocationFailed)
				runtime.allocationFailed(runtime.context,
					static_cast<size_t>(entry.compressedSize) + 1);
			freeFn(output);
			return NULL;
		}

		const fs_offset_t readSize = runtime.read(
			runtime.context, archive.handle, compressed,
			static_cast<size_t>(entry.compressedSize));
		if (readSize != entry.compressedSize)
		{
			if (runtime.sizeMismatch)
				runtime.sizeMismatch(runtime.context, entry.name);
			runtime.tempFree(runtime.context, compressed);
			freeFn(output);
			return NULL;
		}

		if (!runtime.inflateRaw)
		{
			runtime.tempFree(runtime.context, compressed);
			freeFn(output);
			return NULL;
		}

		const bool ok = runtime.inflateRaw(
			runtime.context, compressed,
			static_cast<size_t>(entry.compressedSize),
			output, static_cast<size_t>(entry.size), entry.name);
		runtime.tempFree(runtime.context, compressed);

		if (ok)
		{
			if (fileSize)
				*fileSize = entry.size;
			return output;
		}

		freeFn(output);
		return NULL;
	}

	if (runtime.unsupportedCompression)
		runtime.unsupportedCompression(runtime.context, entry.name);
	freeFn(output);
	return NULL;
}

ZipLoadStatus OpenZipArchive(const ZipOpenRuntime &runtime,
	const char *filename, ZipOpenResult *result)
{
	if (!result)
		return ZipLoadStatus::CouldNotOpen;

	result->handle = NULL;
	result->fileCount = 0;
	result->files = NULL;

	if (!runtime.openSystem || !runtime.close || !runtime.read ||
		!runtime.seek || !runtime.length || !runtime.alloc || !runtime.free)
	{
		return ZipLoadStatus::CouldNotOpen;
	}

	file_t *handle = runtime.openSystem(runtime.context, filename, "rb");
	if (!handle)
		return ZipLoadStatus::CouldNotOpen;

	const fs_offset_t archiveLength = runtime.length(runtime.context, handle);
	if (archiveLength > UINT32_MAX)
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::CouldNotOpen;
	}

	runtime.seek(runtime.context, handle, 0, SEEK_SET);

	uint32_t signature = 0;
	if (!ReadExact(runtime, handle, &signature, sizeof(signature)) ||
		signature == LittleLong(kZipHeaderEndOfCentralDirectory))
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::NoFiles;
	}

	if (signature != LittleLong(kZipHeaderLocalFile))
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::BadHeader;
	}

	fs_offset_t position = archiveLength;
	while (position > 0)
	{
		runtime.seek(runtime.context, handle, position, SEEK_SET);
		if (ReadExact(runtime, handle, &signature, sizeof(signature)) &&
			signature == LittleLong(kZipHeaderEndOfCentralDirectory))
		{
			break;
		}

		position -= sizeof(char);
	}

	if (signature != LittleLong(kZipHeaderEndOfCentralDirectory))
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::BadHeader;
	}

	ZipEndOfCentralDirectory eocd;
	if (!ReadExact(runtime, handle, &eocd, sizeof(eocd)))
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::BadHeader;
	}

	eocd.diskNumber = LittleShort(eocd.diskNumber);
	eocd.startDiskNumber = LittleShort(eocd.startDiskNumber);
	eocd.numberCentralDirectoryRecord =
		LittleShort(eocd.numberCentralDirectoryRecord);
	eocd.totalCentralDirectoryRecord =
		LittleShort(eocd.totalCentralDirectoryRecord);
	eocd.sizeOfCentralDirectory = LittleLong(eocd.sizeOfCentralDirectory);
	eocd.centralDirectoryOffset = LittleLong(eocd.centralDirectoryOffset);
	eocd.commentLength = LittleShort(eocd.commentLength);

	if (eocd.totalCentralDirectoryRecord == 0)
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::NoFiles;
	}

	ZipFileEntry *entries = static_cast<ZipFileEntry *>(
		runtime.alloc(runtime.context,
			static_cast<size_t>(eocd.totalCentralDirectoryRecord) *
				sizeof(ZipFileEntry),
			true));
	if (!entries)
	{
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::Corrupted;
	}

	if (runtime.seek(runtime.context, handle, eocd.centralDirectoryOffset, SEEK_SET) == -1)
	{
		runtime.free(runtime.context, entries);
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::Corrupted;
	}

	int fileCount = 0;
	for (int i = 0; i < eocd.totalCentralDirectoryRecord; ++i)
	{
		ZipCentralDirectoryHeader header;
		if (!ReadExact(runtime, handle, &header, sizeof(header)) ||
			header.signature != LittleLong(kZipHeaderCentralDirectory))
		{
			runtime.free(runtime.context, entries);
			runtime.close(runtime.context, handle);
			return ZipLoadStatus::BadHeader;
		}

		ConvertCentralDirectoryHeader(&header);

		if (header.uncompressedSize && header.filenameLength &&
			header.filenameLength < MAX_SYSPATH)
		{
			char filenameBuffer[MAX_SYSPATH];
			memset(filenameBuffer, 0, sizeof(filenameBuffer));
			if (!ReadExact(runtime, handle, filenameBuffer, header.filenameLength))
			{
				runtime.free(runtime.context, entries);
				runtime.close(runtime.context, handle);
				return ZipLoadStatus::Corrupted;
			}

			CopyString(entries[fileCount].name, sizeof(entries[fileCount].name),
				filenameBuffer);
			entries[fileCount].size = header.uncompressedSize;
			entries[fileCount].compressedSize = header.compressedSize;
			entries[fileCount].offset = header.localHeaderOffset;
			++fileCount;
		}
		else if (runtime.seek(runtime.context, handle,
			header.filenameLength, SEEK_CUR) == -1)
		{
			runtime.free(runtime.context, entries);
			runtime.close(runtime.context, handle);
			return ZipLoadStatus::Corrupted;
		}

		if (header.extraFieldLength &&
			runtime.seek(runtime.context, handle,
				header.extraFieldLength, SEEK_CUR) == -1)
		{
			runtime.free(runtime.context, entries);
			runtime.close(runtime.context, handle);
			return ZipLoadStatus::Corrupted;
		}

		if (header.fileCommentLength &&
			runtime.seek(runtime.context, handle,
				header.fileCommentLength, SEEK_CUR) == -1)
		{
			runtime.free(runtime.context, entries);
			runtime.close(runtime.context, handle);
			return ZipLoadStatus::Corrupted;
		}
	}

	if (fileCount == 0)
	{
		runtime.free(runtime.context, entries);
		runtime.close(runtime.context, handle);
		return ZipLoadStatus::NoFiles;
	}

	for (int i = 0; i < fileCount; ++i)
	{
		ZipLocalHeader header;
		if (runtime.seek(runtime.context, handle, entries[i].offset, SEEK_SET) == -1 ||
			!ReadExact(runtime, handle, &header, sizeof(header)))
		{
			runtime.free(runtime.context, entries);
			runtime.close(runtime.context, handle);
			return ZipLoadStatus::Corrupted;
		}

		ConvertLocalHeader(&header);

		entries[i].flags = header.compressionFlags;
		entries[i].offset = entries[i].offset + header.filenameLength +
			header.extraFieldLength + sizeof(header);
	}

	SortZipEntries(entries, fileCount);

	result->handle = handle;
	result->fileCount = fileCount;
	result->files = entries;

	return ZipLoadStatus::Ok;
}

}
}
