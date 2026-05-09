#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filesystem/wad_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"halflife.wad",
		SearchPathBackendType::Wad,
		4,
		3,
		"game wad"
	};
	return metadata;
}

static bool TestWadBackendMetadata()
{
	WadBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "halflife.wad") &&
		metadata.type == SearchPathBackendType::Wad &&
		metadata.flags == 4 &&
		metadata.order == 3 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "game wad");
}

static bool TestWadBackendPrintInfo()
{
	WadBackend backend(TestMetadata());
	char output[9];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "halflife"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestWadBackendDefaultOperations()
{
	WadBackend backend(TestMetadata());

	if (backend.openFile("probe.txt", "rb", 0))
		return false;

	if (backend.fileTime("probe.txt") != -1)
		return false;

	if (backend.findFile("probe.txt", NULL, 0) != -1)
		return false;

	if (backend.loadFile("probe.txt", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

static bool TestWadTypeMapping()
{
	return WadTypeFromExtension("probe.txt") == TYP_SCRIPT &&
		WadTypeFromExtension("PROBE.TXT") == TYP_SCRIPT &&
		WadTypeFromExtension("probe") == TYP_ANY &&
		WadTypeFromExtension("probe.*") == TYP_ANY &&
		WadTypeFromExtension("probe.bin") == TYP_NONE &&
		ExpectString(WadExtensionFromType(TYP_SCRIPT), "txt") &&
		ExpectString(WadExtensionFromType(TYP_ANY), "") &&
		ExpectString(WadExtensionFromType(TYP_NONE), "");
}

static dlumpinfo_t MakeLump(const char *name, signed char type)
{
	dlumpinfo_t lump;
	memset(&lump, 0, sizeof(lump));
	strncpy(lump.name, name, sizeof(lump.name) - 1);
	lump.type = type;
	return lump;
}

static bool TestWadLumpDirectoryHelpers()
{
	dlumpinfo_t lumps[4];
	int lumpCount = 0;
	bool duplicate = false;
	dlumpinfo_t script = MakeLump("beta", TYP_SCRIPT);
	dlumpinfo_t picture = MakeLump("alpha", TYP_GFXPIC);
	dlumpinfo_t texture = MakeLump("beta", TYP_MIPTEX);
	dlumpinfo_t duplicateScript = MakeLump("beta", TYP_SCRIPT);

	memset(lumps, 0, sizeof(lumps));

	if (!InsertWadLumpSorted(lumps, &lumpCount, "beta", script, &duplicate) ||
		duplicate)
		return false;

	if (!InsertWadLumpSorted(lumps, &lumpCount, "alpha", picture, &duplicate) ||
		duplicate)
		return false;

	if (!InsertWadLumpSorted(lumps, &lumpCount, "beta", texture, &duplicate) ||
		duplicate)
		return false;

	if (lumpCount != 3)
		return false;

	if (!ExpectString(lumps[0].name, "alpha") ||
		!ExpectString(lumps[1].name, "beta") ||
		lumps[1].type != TYP_SCRIPT ||
		!ExpectString(lumps[2].name, "beta") ||
		lumps[2].type != TYP_MIPTEX)
		return false;

	if (FindWadLump(lumps, lumpCount, "BETA", TYP_SCRIPT) != &lumps[1])
		return false;

	if (FindWadLump(lumps, lumpCount, "beta", TYP_ANY) != &lumps[1])
		return false;

	if (FindWadLump(lumps, lumpCount, "beta", TYP_DDSTEX))
		return false;

	if (!InsertWadLumpSorted(lumps, &lumpCount, "beta", duplicateScript,
		&duplicate))
		return false;

	return duplicate && lumpCount == 4;
}

struct MemoryFile
{
	unsigned char *data;
	size_t size;
	size_t position;
};

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

static void *MemoryAlloc(void *, poolhandle_t, size_t size, bool clear)
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

static void WriteLump(unsigned char *dst, const char *name, signed char type)
{
	memset(dst, 0, sizeof(dlumpinfo_t));
	WriteU32(dst + 0, 64);
	WriteU32(dst + 4, 4);
	WriteU32(dst + 8, 4);
	dst[12] = static_cast<unsigned char>(type);
	strncpy(reinterpret_cast<char *>(dst + 16), name, WAD3_NAMELEN - 1);
}

static bool TestWadLumpTableParser()
{
	unsigned char data[sizeof(dwadinfo_t) + 4 * sizeof(dlumpinfo_t)];
	const size_t tableOffset = sizeof(dwadinfo_t);
	MemoryFile memoryFile = {
		data,
		sizeof(data),
		0
	};
	WadLoadRuntime runtime = {
		&memoryFile,
		MemoryRead,
		MemorySeek,
		MemoryAlloc,
		MemoryFree,
		NULL
	};
	WadArchiveTable table;

	memset(data, 0, sizeof(data));
	WriteU32(data + 0, IDWAD3HEADER);
	WriteU32(data + 4, 4);
	WriteU32(data + 8, static_cast<uint32_t>(tableOffset));
	WriteLump(data + tableOffset + 0 * sizeof(dlumpinfo_t), "BETA*", TYP_SCRIPT);
	WriteLump(data + tableOffset + 1 * sizeof(dlumpinfo_t), "alpha", TYP_GFXPIC);
	WriteLump(data + tableOffset + 2 * sizeof(dlumpinfo_t), "CONCHARS", TYP_SCRIPT);
	WriteLump(data + tableOffset + 3 * sizeof(dlumpinfo_t), "beta!", TYP_SCRIPT);

	table.infotableOffset = 0;
	table.lumpCount = 0;
	table.lumps = NULL;

	const WadLoadStatus status = LoadWadLumpTable(
		runtime, "fixture.wad",
		reinterpret_cast<file_t *>(&memoryFile), 0, &table);

	if (status != WadLoadStatus::Ok)
		return false;

	if (table.infotableOffset != static_cast<int>(tableOffset) ||
		table.lumpCount != 4 ||
		!table.lumps)
	{
		MemoryFree(NULL, table.lumps);
		return false;
	}

	const bool ok =
		ExpectString(table.lumps[0].name, "alpha") &&
		ExpectString(table.lumps[1].name, "beta!") &&
		table.lumps[1].type == TYP_SCRIPT &&
		ExpectString(table.lumps[3].name, "conchars") &&
		table.lumps[3].type == TYP_GFXPIC;

	MemoryFree(NULL, table.lumps);
	return ok;
}

int main()
{
	if (!TestWadBackendMetadata() ||
		!TestWadBackendPrintInfo() ||
		!TestWadBackendDefaultOperations() ||
		!TestWadTypeMapping() ||
		!TestWadLumpDirectoryHelpers() ||
		!TestWadLumpTableParser())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
