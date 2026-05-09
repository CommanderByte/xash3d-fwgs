#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filesystem/pak_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"pak0.pak",
		SearchPathBackendType::Pak,
		4,
		2,
		"game archive"
	};
	return metadata;
}

static bool TestPakBackendMetadata()
{
	PakBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "pak0.pak") &&
		metadata.type == SearchPathBackendType::Pak &&
		metadata.flags == 4 &&
		metadata.order == 2 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "game archive");
}

static bool TestPakBackendPrintInfo()
{
	PakBackend backend(TestMetadata());
	char output[8];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "pak0.pa"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestPakBackendDefaultOperations()
{
	PakBackend backend(TestMetadata());

	if (backend.openFile("progs.dat", "rb", 0))
		return false;

	if (backend.fileTime("progs.dat") != -1)
		return false;

	if (backend.findFile("progs.dat", NULL, 0) != -1)
		return false;

	if (backend.loadFile("progs.dat", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

static PakFileEntry MakeEntry(const char *name, int offset, int length)
{
	PakFileEntry entry;
	memset(&entry, 0, sizeof(entry));
	strncpy(entry.name, name, sizeof(entry.name) - 1);
	entry.filepos = offset;
	entry.filelen = length;
	return entry;
}

static bool TestPakEntrySortingAndLookup()
{
	PakFileEntry entries[3];

	entries[0] = MakeEntry("zeta.txt", 20, 3);
	entries[1] = MakeEntry("Alpha.txt", 10, 4);
	entries[2] = MakeEntry("folder/Nested.TXT", 30, 5);

	SortPakEntries(entries, 3);

	PakArchiveView archive = {
		"pak0.pak",
		NULL,
		3,
		entries
	};

	char fixedName[64];
	if (!ExpectString(entries[0].name, "Alpha.txt") ||
		!ExpectString(entries[1].name, "folder/Nested.TXT") ||
		!ExpectString(entries[2].name, "zeta.txt"))
		return false;

	const int index = FindFileInPakArchive(
		archive, "FOLDER/nested.txt", fixedName, sizeof(fixedName));

	return index == 1 &&
		ExpectString(fixedName, "folder/Nested.TXT") &&
		FindFileInPakArchive(archive, "missing.txt", NULL, 0) == -1;
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

static void *MemoryAlloc(void *, size_t size, bool clear)
{
	return clear ? calloc(1, size) : malloc(size);
}

static void MemoryFree(void *, void *memory)
{
	free(memory);
}

static void WriteU32(unsigned char *dst, uint32_t value)
{
	dst[0] = static_cast<unsigned char>(value & 0xff);
	dst[1] = static_cast<unsigned char>((value >> 8) & 0xff);
	dst[2] = static_cast<unsigned char>((value >> 16) & 0xff);
	dst[3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

static void WriteEntry(unsigned char *dst, const char *name, int offset, int length)
{
	memset(dst, 0, sizeof(PakFileEntry));
	strncpy(reinterpret_cast<char *>(dst), name, 55);
	WriteU32(dst + 56, static_cast<uint32_t>(offset));
	WriteU32(dst + 60, static_cast<uint32_t>(length));
}

static bool TestPakArchiveParser()
{
	const size_t headerSize = 12;
	const size_t dirOffset = headerSize;
	unsigned char data[headerSize + 2 * sizeof(PakFileEntry)];
	MemoryFile memoryFile = {
		data,
		sizeof(data),
		0,
		false
	};
	PakOpenRuntime runtime = {
		&memoryFile,
		MemoryOpen,
		MemoryClose,
		MemoryRead,
		MemorySeek,
		MemoryAlloc,
		MemoryFree
	};
	PakOpenResult result;

	memset(data, 0, sizeof(data));
	WriteU32(data + 0, (('K' << 24) + ('C' << 16) + ('A' << 8) + 'P'));
	WriteU32(data + 4, static_cast<uint32_t>(dirOffset));
	WriteU32(data + 8, static_cast<uint32_t>(2 * sizeof(PakFileEntry)));
	WriteEntry(data + dirOffset, "zeta.txt", 128, 8);
	WriteEntry(data + dirOffset + sizeof(PakFileEntry), "alpha.txt", 64, 4);

	const PakLoadStatus status = OpenPakArchive(runtime, "fixture.pak", &result);

	if (status != PakLoadStatus::Ok ||
		result.handle != reinterpret_cast<file_t *>(&memoryFile) ||
		result.fileCount != 2 ||
		!result.files)
	{
		return false;
	}

	const bool ok = ExpectString(result.files[0].name, "alpha.txt") &&
		result.files[0].filepos == 64 &&
		result.files[0].filelen == 4 &&
		ExpectString(result.files[1].name, "zeta.txt") &&
		result.files[1].filepos == 128 &&
		result.files[1].filelen == 8;

	MemoryFree(NULL, result.files);
	return ok;
}

int main()
{
	if (!TestPakBackendMetadata() ||
		!TestPakBackendPrintInfo() ||
		!TestPakBackendDefaultOperations() ||
		!TestPakEntrySortingAndLookup() ||
		!TestPakArchiveParser())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
