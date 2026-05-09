#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filesystem/zip_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"extras.pk3",
		SearchPathBackendType::Zip,
		4,
		4,
		"game pk3"
	};
	return metadata;
}

static bool TestZipBackendMetadata()
{
	ZipBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "extras.pk3") &&
		metadata.type == SearchPathBackendType::Zip &&
		metadata.flags == 4 &&
		metadata.order == 4 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "game pk3");
}

static bool TestZipBackendPrintInfo()
{
	ZipBackend backend(TestMetadata());
	char output[7];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "extras"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestZipBackendDefaultOperations()
{
	ZipBackend backend(TestMetadata());

	if (backend.openFile("stored.txt", "rb", 0))
		return false;

	if (backend.fileTime("stored.txt") != -1)
		return false;

	if (backend.findFile("stored.txt", NULL, 0) != -1)
		return false;

	if (backend.loadFile("stored.txt", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

static ZipFileEntry MakeEntry(const char *name, int offset, int length)
{
	ZipFileEntry entry;
	memset(&entry, 0, sizeof(entry));
	strncpy(entry.name, name, sizeof(entry.name) - 1);
	entry.offset = offset;
	entry.size = length;
	entry.compressedSize = length;
	entry.flags = static_cast<unsigned short>(ZipCompression::Stored);
	return entry;
}

static bool TestZipEntrySortingAndLookup()
{
	ZipFileEntry entries[3];

	entries[0] = MakeEntry("zeta.txt", 30, 3);
	entries[1] = MakeEntry("Alpha.txt", 10, 4);
	entries[2] = MakeEntry("folder/Nested.TXT", 20, 5);

	SortZipEntries(entries, 3);

	ZipArchiveView archive = {
		"extras.pk3",
		NULL,
		3,
		entries
	};

	char fixedName[64];
	if (!ExpectString(entries[0].name, "Alpha.txt") ||
		!ExpectString(entries[1].name, "folder/Nested.TXT") ||
		!ExpectString(entries[2].name, "zeta.txt"))
		return false;

	const int index = FindFileInZipArchive(
		archive, "FOLDER/nested.txt", fixedName, sizeof(fixedName));

	return index == 1 &&
		ExpectString(fixedName, "folder/Nested.TXT") &&
		FindFileInZipArchive(archive, "missing.txt", NULL, 0) == -1;
}

struct MemoryFile
{
	unsigned char *data;
	size_t size;
	size_t position;
	bool closed;
};

static file_t *MemoryOpen(void *context, const char *, const char *)
{
	return reinterpret_cast<file_t *>(context);
}

static int MemoryClose(void *context, file_t *)
{
	MemoryFile *file = static_cast<MemoryFile *>(context);
	file->closed = true;
	return 0;
}

static fs_offset_t MemoryRead(void *context, file_t *, void *buffer, size_t size)
{
	MemoryFile *file = static_cast<MemoryFile *>(context);

	if (file->position + size > file->size)
		return -1;

	memcpy(buffer, file->data + file->position, size);
	file->position += size;
	return static_cast<fs_offset_t>(size);
}

static int MemorySeek(void *context, file_t *, fs_offset_t offset, int whence)
{
	MemoryFile *file = static_cast<MemoryFile *>(context);
	size_t nextPosition;

	if (whence == SEEK_SET)
		nextPosition = static_cast<size_t>(offset);
	else if (whence == SEEK_CUR)
		nextPosition = file->position + static_cast<size_t>(offset);
	else if (whence == SEEK_END)
		nextPosition = file->size + static_cast<size_t>(offset);
	else
		return -1;

	if (nextPosition > file->size)
		return -1;

	file->position = nextPosition;
	return 0;
}

static fs_offset_t MemoryLength(void *context, file_t *)
{
	MemoryFile *file = static_cast<MemoryFile *>(context);
	return static_cast<fs_offset_t>(file->size);
}

static void *MemoryAlloc(void *, size_t size, bool clear)
{
	return clear ? calloc(1, size) : malloc(size);
}

static void MemoryFree(void *, void *memory)
{
	free(memory);
}

static void *MemoryTempAlloc(void *, size_t size)
{
	return malloc(size);
}

static void WriteU16(unsigned char *dst, uint16_t value)
{
	dst[0] = static_cast<unsigned char>(value & 0xff);
	dst[1] = static_cast<unsigned char>((value >> 8) & 0xff);
}

static void WriteU32(unsigned char *dst, uint32_t value)
{
	dst[0] = static_cast<unsigned char>(value & 0xff);
	dst[1] = static_cast<unsigned char>((value >> 8) & 0xff);
	dst[2] = static_cast<unsigned char>((value >> 16) & 0xff);
	dst[3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

static size_t WriteLocalHeader(unsigned char *dst, const char *name,
	size_t payloadSize)
{
	const size_t nameLength = strlen(name);
	WriteU32(dst + 0, 0x04034b50U);
	WriteU16(dst + 4, 20);
	WriteU16(dst + 6, 0);
	WriteU16(dst + 8, 0);
	WriteU16(dst + 10, 0);
	WriteU16(dst + 12, 0);
	WriteU32(dst + 14, 0);
	WriteU32(dst + 18, static_cast<uint32_t>(payloadSize));
	WriteU32(dst + 22, static_cast<uint32_t>(payloadSize));
	WriteU16(dst + 26, static_cast<uint16_t>(nameLength));
	WriteU16(dst + 28, 0);
	memcpy(dst + 30, name, nameLength);
	return 30 + nameLength;
}

static size_t WriteCentralHeader(unsigned char *dst, const char *name,
	size_t payloadSize, uint32_t localOffset)
{
	const size_t nameLength = strlen(name);
	WriteU32(dst + 0, 0x02014b50U);
	WriteU16(dst + 4, 20);
	WriteU16(dst + 6, 20);
	WriteU16(dst + 8, 0);
	WriteU16(dst + 10, 0);
	WriteU16(dst + 12, 0);
	WriteU16(dst + 14, 0);
	WriteU32(dst + 16, 0);
	WriteU32(dst + 20, static_cast<uint32_t>(payloadSize));
	WriteU32(dst + 24, static_cast<uint32_t>(payloadSize));
	WriteU16(dst + 28, static_cast<uint16_t>(nameLength));
	WriteU16(dst + 30, 0);
	WriteU16(dst + 32, 0);
	WriteU16(dst + 34, 0);
	WriteU16(dst + 36, 0);
	WriteU32(dst + 38, 0);
	WriteU32(dst + 42, localOffset);
	memcpy(dst + 46, name, nameLength);
	return 46 + nameLength;
}

static size_t WriteEocd(unsigned char *dst, uint16_t count,
	uint32_t centralSize, uint32_t centralOffset)
{
	WriteU32(dst + 0, 0x06054b50U);
	WriteU16(dst + 4, 0);
	WriteU16(dst + 6, 0);
	WriteU16(dst + 8, count);
	WriteU16(dst + 10, count);
	WriteU32(dst + 12, centralSize);
	WriteU32(dst + 16, centralOffset);
	WriteU16(dst + 20, 0);
	return 22;
}

static bool TestZipArchiveParserAndStoredLoad()
{
	const char name[] = "stored.txt";
	const char payload[] = "stored payload";
	unsigned char data[256];
	size_t position = 0;
	const uint32_t localOffset = 0;

	memset(data, 0, sizeof(data));
	position += WriteLocalHeader(data + position, name, sizeof(payload) - 1);
	memcpy(data + position, payload, sizeof(payload) - 1);
	position += sizeof(payload) - 1;

	const uint32_t centralOffset = static_cast<uint32_t>(position);
	position += WriteCentralHeader(data + position, name,
		sizeof(payload) - 1, localOffset);
	const uint32_t centralSize = static_cast<uint32_t>(position - centralOffset);
	position += WriteEocd(data + position, 1, centralSize, centralOffset);

	MemoryFile memoryFile = {
		data,
		position,
		0,
		false
	};
	ZipOpenRuntime openRuntime = {
		&memoryFile,
		MemoryOpen,
		MemoryClose,
		MemoryRead,
		MemorySeek,
		MemoryLength,
		MemoryAlloc,
		MemoryFree
	};
	ZipOpenResult result;

	const ZipLoadStatus status = OpenZipArchive(
		openRuntime, "fixture.pk3", &result);

	if (status != ZipLoadStatus::Ok ||
		result.handle != reinterpret_cast<file_t *>(&memoryFile) ||
		result.fileCount != 1 ||
		!result.files ||
		!ExpectString(result.files[0].name, name))
	{
		return false;
	}

	ZipArchiveView archive = {
		"fixture.pk3",
		reinterpret_cast<file_t *>(&memoryFile),
		result.fileCount,
		result.files
	};
	ZipLoadFileRuntime loadRuntime = {
		&memoryFile,
		MemorySeek,
		MemoryRead,
		MemoryTempAlloc,
		MemoryFree,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};
	fs_offset_t loadedSize = 0;
	byte *loaded = LoadZipEntry(
		archive, loadRuntime, 0, &loadedSize, malloc, free);

	const bool ok = loaded &&
		loadedSize == static_cast<fs_offset_t>(sizeof(payload) - 1) &&
		memcmp(loaded, payload, sizeof(payload) - 1) == 0;

	free(loaded);
	MemoryFree(NULL, result.files);
	return ok;
}

int main()
{
	if (!TestZipBackendMetadata() ||
		!TestZipBackendPrintInfo() ||
		!TestZipBackendDefaultOperations() ||
		!TestZipEntrySortingAndLookup() ||
		!TestZipArchiveParserAndStoredLoad())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
