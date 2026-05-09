#include "filesystem/wad_backend.hpp"

#include <ctype.h>
#include <stdio.h>
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

struct WadTypeMapping
{
	const char *extension;
	signed char type;
};

const WadTypeMapping kWadTypes[] = {
	{ "pal", TYP_PALETTE },
	{ "dds", TYP_DDSTEX },
	{ "lmp", TYP_GFXPIC },
	{ "fnt", TYP_QFONT },
	{ "mip", TYP_MIPTEX },
	{ "txt", TYP_SCRIPT },
};

const int kMaxFilesInWad = 65535;

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

const char *FileExtension(const char *path)
{
	const char *lastDot = NULL;
	const char *lastSeparator = NULL;

	if (!path)
		return "";

	for (const char *current = path; *current; ++current)
	{
		if (*current == '/' || *current == '\\' || *current == ':')
			lastSeparator = current;
		else if (*current == '.')
			lastDot = current;
	}

	if (!lastDot || (lastSeparator && lastDot < lastSeparator))
		return "";

	return lastDot + 1;
}

const char *LastPathElement(const char *path)
{
	const char *last = path ? path : "";

	for (const char *current = last; *current; ++current)
	{
		if (*current == '/' || *current == '\\' || *current == ':')
			last = current + 1;
	}

	return last;
}

const char *FileWithoutTrailingPath(const char *path)
{
	const char *base = LastPathElement(path);
	return base && base[0] ? base : path;
}

void ExtractFilePath(const char *path, char *dst, size_t size)
{
	if (!dst || size == 0)
		return;

	dst[0] = '\0';
	if (!path)
		return;

	const char *lastSeparator = NULL;
	for (const char *current = path; *current; ++current)
	{
		if (*current == '/' || *current == '\\' || *current == ':')
			lastSeparator = current;
	}

	if (!lastSeparator)
		return;

	const size_t length = static_cast<size_t>(lastSeparator - path + 1);
	const size_t copyLength = length < size - 1 ? length : size - 1;
	memcpy(dst, path, copyLength);
	dst[copyLength] = '\0';
}

void FileBase(const char *path, char *dst, size_t size)
{
	if (!dst || size == 0)
		return;

	dst[0] = '\0';
	if (!path)
		return;

	const char *end = path + strlen(path);
	while (end > path && (end[-1] == '/' || end[-1] == '\\' || end[-1] == ':'))
		--end;

	const char *base = path;
	for (const char *current = path; current < end; ++current)
	{
		if (*current == '/' || *current == '\\' || *current == ':')
			base = current + 1;
	}

	for (const char *current = base; current < end; ++current)
	{
		if (*current == '.')
			end = current;
	}

	size_t length = static_cast<size_t>(end - base);
	if (length > size - 1)
		length = size - 1;

	memcpy(dst, base, length);
	dst[length] = '\0';
}

bool HasExtension(const char *path)
{
	const char *extension = FileExtension(path);
	return extension && extension[0];
}

void DefaultExtension(char *path, const char *extension, size_t size)
{
	if (!path || !extension || size == 0 || HasExtension(path))
		return;

	const size_t pathLength = strlen(path);
	if (pathLength >= size - 1)
		return;

	size_t extensionLength = strlen(extension);
	if (extensionLength > size - pathLength - 1)
		extensionLength = size - pathLength - 1;

	memcpy(path + pathLength, extension, extensionLength);
	path[pathLength + extensionLength] = '\0';
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

void CopyLumpName(char *dst, const char *src)
{
	memset(dst, 0, WAD3_NAMELEN);

	if (!src)
		return;

	for (int i = 0; i < WAD3_NAMELEN && src[i]; ++i)
		dst[i] = src[i];
}

void NormalizeLumpName(char *dst, const char *src, size_t srcSize)
{
	memset(dst, 0, WAD3_NAMELEN);

	if (!src)
		return;

	size_t lastStar = WAD3_NAMELEN;
	size_t i = 0;
	for (; i + 1 < WAD3_NAMELEN && i < srcSize && src[i]; ++i)
	{
		dst[i] = static_cast<char>(tolower(static_cast<unsigned char>(src[i])));
		if (dst[i] == '*')
			lastStar = i;
	}

	if (lastStar != WAD3_NAMELEN)
		dst[lastStar] = '!';
}

bool IsConcharsLump(const char *name)
{
	return CaseInsensitiveCompare(name, "conchars") == 0;
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

signed char WadTypeFromExtension(const char *path)
{
	const char *extension = FileExtension(path);

	if (CaseInsensitiveCompare(extension, "*") == 0 || !extension[0])
		return TYP_ANY;

	for (size_t i = 0; i < sizeof(kWadTypes) / sizeof(kWadTypes[0]); ++i)
	{
		if (CaseInsensitiveCompare(extension, kWadTypes[i].extension) == 0)
			return kWadTypes[i].type;
	}

	return TYP_NONE;
}

const char *WadExtensionFromType(signed char lumpType)
{
	if (lumpType == TYP_NONE || lumpType == TYP_ANY)
		return "";

	for (size_t i = 0; i < sizeof(kWadTypes) / sizeof(kWadTypes[0]); ++i)
	{
		if (lumpType == kWadTypes[i].type)
			return kWadTypes[i].extension;
	}

	return "";
}

dlumpinfo_t *FindWadLump(dlumpinfo_t *lumps, int lumpCount,
	const char *name, signed char matchType)
{
	if (!lumps || lumpCount <= 0 || matchType == TYP_NONE)
		return NULL;

	int left = 0;
	int right = lumpCount - 1;

	while (left <= right)
	{
		const int middle = (left + right) / 2;
		int diff = CaseInsensitiveCompare(lumps[middle].name, name);

		if (!diff)
		{
			if (matchType == TYP_ANY || matchType == lumps[middle].type)
				return &lumps[middle];
			if (lumps[middle].type < matchType)
				diff = 1;
			else if (lumps[middle].type > matchType)
				diff = -1;
			else
				break;
		}

		if (diff > 0)
			right = middle - 1;
		else
			left = middle + 1;
	}

	return NULL;
}

dlumpinfo_t *InsertWadLumpSorted(dlumpinfo_t *lumps, int *lumpCount,
	const char *name, const dlumpinfo_t &newLump, bool *duplicateExact)
{
	if (!lumps || !lumpCount || *lumpCount < 0)
		return NULL;

	if (duplicateExact)
		*duplicateExact = false;

	int left = 0;
	int right = *lumpCount - 1;

	while (left <= right)
	{
		const int middle = (left + right) / 2;
		int diff = CaseInsensitiveCompare(lumps[middle].name, name);

		if (!diff)
		{
			if (lumps[middle].type < newLump.type)
				diff = 1;
			else if (lumps[middle].type > newLump.type)
				diff = -1;
			else if (duplicateExact)
				*duplicateExact = true;
		}

		if (diff > 0)
			right = middle - 1;
		else
			left = middle + 1;
	}

	dlumpinfo_t *target = &lumps[left];
	memmove(target + 1, target, (*lumpCount - left) * sizeof(*target));
	++(*lumpCount);

	*target = newLump;
	CopyLumpName(target->name, name);

	return target;
}

int FindFileInWadArchive(const WadArchiveView &archive, const char *path,
	char *fixedName, size_t fixedNameSize)
{
	const signed char type = WadTypeFromExtension(path);
	bool anyWadName = true;
	char wadName[256];
	char shortName[256];

	if (type == TYP_NONE)
		return -1;

	ExtractFilePath(path, wadName, sizeof(wadName));
	if (wadName[0])
	{
		char wadBaseName[256];

		FileBase(wadName, wadBaseName, sizeof(wadBaseName));
		snprintf(wadName, sizeof(wadName), "%s.wad", wadBaseName);
		anyWadName = false;
	}

	FileBase(archive.source, shortName, sizeof(shortName));
	DefaultExtension(shortName, ".wad", sizeof(shortName));

	if (!anyWadName && CaseInsensitiveCompare(wadName, shortName) != 0)
		return -1;

	FileBase(path, shortName, sizeof(shortName));

	dlumpinfo_t *lump = FindWadLump(archive.lumps, archive.lumpCount,
		shortName, type);

	if (!lump)
		return -1;

	if (fixedName)
		CopyString(fixedName, fixedNameSize, lump->name);

	return static_cast<int>(lump - archive.lumps);
}

void SearchWadArchive(const WadArchiveView &archive,
	const WadSearchRuntime &runtime, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	const signed char type = WadTypeFromExtension(pattern);
	bool anyWadName = true;
	char wadPattern[256];
	char wadName[256];
	char wadFolder[256];
	char archiveBaseName[256];

	if (type == TYP_NONE || !runtime.matchPattern || !runtime.stringCount ||
		!runtime.stringAt || !runtime.append)
	{
		return;
	}

	ExtractFilePath(pattern, wadName, sizeof(wadName));
	FileBase(pattern, wadPattern, sizeof(wadPattern));
	wadFolder[0] = '\0';

	if (wadName[0])
	{
		char wadBaseName[256];

		FileBase(wadName, wadBaseName, sizeof(wadBaseName));
		CopyString(wadFolder, sizeof(wadFolder), wadBaseName);
		snprintf(wadName, sizeof(wadName), "%s.wad", wadBaseName);
		anyWadName = false;
	}

	FileBase(archive.source, archiveBaseName, sizeof(archiveBaseName));
	DefaultExtension(archiveBaseName, ".wad", sizeof(archiveBaseName));

	if (!anyWadName && CaseInsensitiveCompare(wadName, archiveBaseName) != 0)
		return;

	for (int i = 0; i < archive.lumpCount; ++i)
	{
		if (type != TYP_ANY && archive.lumps[i].type != type)
			continue;

		char temp[256];
		CopyString(temp, sizeof(temp), archive.lumps[i].name);

		while (temp[0])
		{
			if (runtime.matchPattern(runtime.context, temp, wadPattern, true))
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
				{
					char result[512];
					char extension[16];
					snprintf(result, sizeof(result), "%s/%s", wadFolder, temp);
					snprintf(extension, sizeof(extension), ".%s",
						WadExtensionFromType(archive.lumps[i].type));
					DefaultExtension(result, extension, sizeof(result));
					runtime.append(runtime.context, list, result);
				}
			}

			StripLastPathElement(temp);
		}
	}

	(void)caseInsensitive;
}

WadLoadStatus LoadWadLumpTable(const WadLoadRuntime &runtime,
	const char *wadFile, file_t *handle, poolhandle_t pool,
	WadArchiveTable *table)
{
	if (!runtime.read || !runtime.seek || !runtime.alloc || !runtime.free ||
		!table || !handle)
	{
		return WadLoadStatus::CouldNotOpen;
	}

	table->infotableOffset = 0;
	table->lumpCount = 0;
	table->lumps = NULL;

	dwadinfo_t header;
	if (runtime.read(runtime.context, handle, &header, sizeof(header)) !=
		static_cast<fs_offset_t>(sizeof(header)))
	{
		return WadLoadStatus::BadHeader;
	}

	if (header.ident != LittleLong(IDWAD2HEADER) &&
		header.ident != LittleLong(IDWAD3HEADER))
	{
		return WadLoadStatus::BadHeader;
	}

	header.ident = LittleLong(header.ident);
	header.numlumps = LittleLong(header.numlumps);
	header.infotableofs = LittleLong(header.infotableofs);

	WadLoadStatus status = WadLoadStatus::Ok;
	const int lumpCount = header.numlumps;
	if (lumpCount >= kMaxFilesInWad)
	{
		status = WadLoadStatus::TooManyFiles;
	}
	else if (lumpCount <= 0)
	{
		return WadLoadStatus::NoFiles;
	}

	table->infotableOffset = header.infotableofs;

	if (runtime.seek(runtime.context, handle, table->infotableOffset, SEEK_SET) == -1)
		return WadLoadStatus::BadFolders;

	const size_t tableSize = static_cast<size_t>(lumpCount) * sizeof(dlumpinfo_t);
	dlumpinfo_t *sourceLumps = static_cast<dlumpinfo_t *>(
		runtime.alloc(runtime.context, pool, tableSize, false));
	if (!sourceLumps)
		return WadLoadStatus::Corrupted;

	if (runtime.read(runtime.context, handle, sourceLumps, tableSize) !=
		static_cast<fs_offset_t>(tableSize))
	{
		runtime.free(runtime.context, sourceLumps);
		return WadLoadStatus::Corrupted;
	}

	for (int i = 0; i < lumpCount; ++i)
	{
		sourceLumps[i].filepos = LittleLong(sourceLumps[i].filepos);
		sourceLumps[i].disksize = LittleLong(sourceLumps[i].disksize);
		sourceLumps[i].size = LittleLong(sourceLumps[i].size);
	}

	dlumpinfo_t *sortedLumps = static_cast<dlumpinfo_t *>(
		runtime.alloc(runtime.context, pool, tableSize, true));
	if (!sortedLumps)
	{
		runtime.free(runtime.context, sourceLumps);
		return WadLoadStatus::Corrupted;
	}

	table->lumps = sortedLumps;
	table->lumpCount = 0;

	for (int i = 0; i < lumpCount; ++i)
	{
		char normalizedName[WAD3_NAMELEN];
		bool duplicate = false;

		NormalizeLumpName(normalizedName, sourceLumps[i].name,
			sizeof(sourceLumps[i].name));

		if (sourceLumps[i].type == TYP_SCRIPT &&
			IsConcharsLump(normalizedName))
		{
			sourceLumps[i].type = TYP_GFXPIC;
		}

		InsertWadLumpSorted(sortedLumps, &table->lumpCount,
			normalizedName, sourceLumps[i], &duplicate);

		if (duplicate && runtime.duplicateLump)
			runtime.duplicateLump(runtime.context, wadFile, normalizedName);
	}

	runtime.free(runtime.context, sourceLumps);
	return status;
}

byte *ReadWadLump(const WadArchiveView &archive, const WadReadRuntime &runtime,
	int lumpIndex, fs_offset_t *lumpSize, void *(*alloc)(size_t),
	void (*freeFn)(void *))
{
	if (lumpSize)
		*lumpSize = 0;

	if (!archive.lumps || !archive.handle || lumpIndex < 0 ||
		lumpIndex >= archive.lumpCount || !runtime.tell || !runtime.seek ||
		!runtime.read || !alloc || !freeFn)
	{
		return NULL;
	}

	const dlumpinfo_t *lump = &archive.lumps[lumpIndex];
	const fs_offset_t oldPosition = runtime.tell(runtime.context, archive.handle);

	if (runtime.seek(runtime.context, archive.handle, lump->filepos, SEEK_SET) == -1)
	{
		if (runtime.corrupted)
			runtime.corrupted(runtime.context, lump->name);
		runtime.seek(runtime.context, archive.handle, oldPosition, SEEK_SET);
		return NULL;
	}

	byte *buffer = static_cast<byte *>(alloc(lump->disksize));
	if (!buffer)
	{
		if (runtime.allocationFailed)
			runtime.allocationFailed(runtime.context, lump->disksize);
		runtime.seek(runtime.context, archive.handle, oldPosition, SEEK_SET);
		return NULL;
	}

	const fs_offset_t size = runtime.read(
		runtime.context, archive.handle, buffer, lump->disksize);
	runtime.seek(runtime.context, archive.handle, oldPosition, SEEK_SET);

	if (size < lump->disksize)
	{
		if (runtime.shortRead)
			runtime.shortRead(runtime.context, lump->name);
		freeFn(buffer);
		return NULL;
	}

	if (lumpSize)
		*lumpSize = lump->disksize;

	return buffer;
}

WadLoadStatus OpenWadArchive(const WadOpenRuntime &runtime,
	const char *filename, bool packed, WadOpenResult *result)
{
	if (!result)
		return WadLoadStatus::CouldNotOpen;

	result->handle = NULL;
	result->pool = 0;
	result->fileTime = 0;
	result->table.infotableOffset = 0;
	result->table.lumpCount = 0;
	result->table.lumps = NULL;

	if ((!packed && !runtime.openSystem) ||
		(packed && !runtime.openPacked) ||
		!runtime.allocPool || !runtime.freePool || !runtime.close)
	{
		return WadLoadStatus::CouldNotOpen;
	}

	file_t *handle = packed
		? runtime.openPacked(runtime.context, FileWithoutTrailingPath(filename))
		: runtime.openSystem(runtime.context, filename, "rb");

	if (!handle)
		return WadLoadStatus::CouldNotOpen;

	poolhandle_t pool = runtime.allocPool(runtime.context, filename);
	WadArchiveTable table = {
		0,
		0,
		NULL
	};
	const WadLoadStatus status = LoadWadLumpTable(
		runtime.loadRuntime, filename, handle, pool, &table);

	if (status != WadLoadStatus::Ok && status != WadLoadStatus::TooManyFiles)
	{
		runtime.freePool(runtime.context, &pool);
		runtime.close(runtime.context, handle);
		return status;
	}

	result->handle = handle;
	result->pool = pool;
	result->fileTime = runtime.fileTime
		? runtime.fileTime(runtime.context, filename)
		: 0;
	result->table = table;

	return status;
}

}
}
