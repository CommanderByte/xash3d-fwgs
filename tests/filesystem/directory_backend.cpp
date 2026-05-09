#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "filesystem/directory_backend.hpp"

using namespace xash::filesystem;

struct stringlist_s
{
	int maxstrings;
	int numstrings;
	char **strings;
};

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static void TestCopyString(char *dst, size_t size, const char *src)
{
	if (!dst || size == 0)
		return;

	size_t i = 0;
	for (; i + 1 < size && src[i]; ++i)
		dst[i] = src[i];
	dst[i] = '\0';
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"valve/",
		SearchPathBackendType::Directory,
		4,
		12,
		"gamefolder"
	};
	return metadata;
}

static bool TestSearchPathBackendTypeNames()
{
	return ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Directory), "directory") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Pak), "pak") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Wad), "wad") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Zip), "zip") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Pk3Directory), "pk3dir") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::AndroidAssets), "android_assets") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Unknown), "unknown");
}

static bool TestDirectoryBackendMetadata()
{
	DirectoryBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "valve/") &&
		metadata.type == SearchPathBackendType::Directory &&
		metadata.flags == 4 &&
		metadata.order == 12 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "gamefolder");
}

static bool TestDirectoryBackendPrintInfo()
{
	DirectoryBackend backend(TestMetadata());
	char output[8];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "valve/"))
		return false;

	char truncated[4];
	backend.printInfo(truncated, sizeof(truncated));

	return ExpectString(truncated, "val");
}

static bool TestDirectoryBackendDefaultOperations()
{
	DirectoryBackend backend(TestMetadata());

	if (backend.openFile("maps/c0a0.bsp", "rb", 0))
		return false;

	if (backend.fileTime("maps/c0a0.bsp") != -1)
		return false;

	if (backend.findFile("maps/c0a0.bsp", NULL, 0) != -1)
		return false;

	if (backend.loadFile("maps/c0a0.bsp", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

struct FakeRuntime
{
	bool includeDirectFile;
	int overflowCount;
};

static char *DuplicateString(const char *text)
{
	const size_t length = strlen(text) + 1;
	char *copy = static_cast<char *>(malloc(length));
	if (copy)
		memcpy(copy, text, length);
	return copy;
}

static void *FakeAlloc(void *, size_t size, bool clear)
{
	void *memory = malloc(size);
	if (memory && clear)
		memset(memory, 0, size);
	return memory;
}

static void FakeFree(void *, void *memory)
{
	free(memory);
}

static bool FakeFolderExists(void *, const char *path)
{
	return ExpectString(path, "root/") ||
		ExpectString(path, "root/Folder") ||
		ExpectString(path, "root/Folder/");
}

static bool FakeFileExists(void *, const char *path)
{
	return ExpectString(path, "root/Folder/File.TXT") ||
		ExpectString(path, "root/Direct.BIN");
}

static bool FakeFileOrFolderExists(void *context, const char *path)
{
	FakeRuntime *runtime = static_cast<FakeRuntime *>(context);

	if (FakeFileExists(context, path) || FakeFolderExists(context, path))
		return true;

	return runtime && runtime->includeDirectFile &&
		ExpectString(path, "root/Direct.BIN");
}

static bool FakeDirectoryCaseSensitive(void *, const char *)
{
	return true;
}

static void AddListString(stringlist_t *list, const char *text)
{
	char **strings = static_cast<char **>(realloc(list->strings,
		sizeof(char *) * static_cast<size_t>(list->numstrings + 1)));
	if (!strings)
		return;

	list->strings = strings;
	list->strings[list->numstrings++] = DuplicateString(text);
}

static stringlist_t *FakeListCreate(void *)
{
	return static_cast<stringlist_t *>(calloc(1, sizeof(stringlist_t)));
}

static void FakeListDirectory(void *context, stringlist_t *list,
	const char *path, bool)
{
	FakeRuntime *runtime = static_cast<FakeRuntime *>(context);

	if (ExpectString(path, "root/"))
	{
		AddListString(list, "Folder");
		if (runtime && runtime->includeDirectFile)
			AddListString(list, "Direct.BIN");
	}
	else if (ExpectString(path, "root/Folder") ||
		ExpectString(path, "root/Folder/"))
	{
		AddListString(list, "File.TXT");
	}
}

static void FakeListDestroy(void *, stringlist_t *list)
{
	if (!list)
		return;

	for (int i = 0; i < list->numstrings; ++i)
		free(list->strings[i]);
	free(list->strings);
	free(list);
}

static int FakeStringCount(void *, stringlist_t *list)
{
	return list ? list->numstrings : 0;
}

static const char *FakeStringAt(void *, stringlist_t *list, int index)
{
	if (!list || index < 0 || index >= list->numstrings)
		return NULL;

	return list->strings[index];
}

static void FakeOverflow(void *context, const char *, const char *)
{
	FakeRuntime *runtime = static_cast<FakeRuntime *>(context);
	if (runtime)
		++runtime->overflowCount;
}

static DirectoryCaseRuntime MakeFakeCaseRuntime(FakeRuntime *fake)
{
	DirectoryCaseRuntime runtime = {
		fake,
		FakeAlloc,
		FakeFree,
		FakeFolderExists,
		FakeFileExists,
		FakeFileOrFolderExists,
		FakeDirectoryCaseSensitive,
		FakeListCreate,
		FakeListDirectory,
		FakeListDestroy,
		FakeStringCount,
		FakeStringAt,
		FakeOverflow
	};
	return runtime;
}

static bool CaseInsensitiveEquals(const char *left, const char *right)
{
	while (*left && *right)
	{
		if (tolower(static_cast<unsigned char>(*left)) !=
			tolower(static_cast<unsigned char>(*right)))
		{
			return false;
		}
		++left;
		++right;
	}

	return *left == *right;
}

static bool FakeMatchPattern(void *, const char *text, const char *pattern,
	bool)
{
	if (!CaseInsensitiveEquals(pattern, "folder/*.txt"))
		return false;

	const char prefix[] = "folder/";
	const size_t prefixLength = sizeof(prefix) - 1;
	const size_t textLength = strlen(text);
	if (textLength < prefixLength + 4)
		return false;

	for (size_t i = 0; i < prefixLength; ++i)
	{
		if (tolower(static_cast<unsigned char>(text[i])) != prefix[i])
			return false;
	}

	return CaseInsensitiveEquals(text + textLength - 4, ".txt");
}

static int ResultStringCount(void *, stringlist_t *list)
{
	return FakeStringCount(NULL, list);
}

static const char *ResultStringAt(void *, stringlist_t *list, int index)
{
	return FakeStringAt(NULL, list, index);
}

static void ResultAppend(void *, stringlist_t *list, const char *text)
{
	AddListString(list, text);
}

static bool TestDirectoryCaseRepairHelpers()
{
	FakeRuntime fake = { false, 0 };
	DirectoryCaseRuntime runtime = MakeFakeCaseRuntime(&fake);
	DirectoryEntry root;
	TestCopyString(root.name, sizeof(root.name), "root/");
	root.entryCount = DirectoryEntryNotScanned;
	root.entries = NULL;
	root.backend = NULL;

	char fixed[MAX_SYSPATH];
	if (!FixDirectoryFileCase(&root, runtime, "folder/file.txt", fixed,
		sizeof(fixed), false))
	{
		return false;
	}

	if (!ExpectString(fixed, "root/Folder/File.TXT"))
		return false;

	char fixedName[MAX_SYSPATH];
	if (FindFileInDirectory(&root, runtime, "root/", "FOLDER/FILE.txt",
		fixedName, sizeof(fixedName)) != 0)
	{
		return false;
	}

	if (!ExpectString(fixedName, "Folder/File.TXT"))
		return false;

	if (FindFileInDirectory(&root, runtime, "root/", "direct.bin",
		fixedName, sizeof(fixedName)) != -1)
	{
		return false;
	}

	fake.includeDirectFile = true;
	if (FindFileInDirectory(&root, runtime, "root/", "direct.bin",
		fixedName, sizeof(fixedName)) != 0)
	{
		return false;
	}

	if (!ExpectString(fixedName, "Direct.BIN"))
		return false;

	FreeDirectoryEntries(&root, runtime);
	return true;
}

static bool TestDirectorySearchHelpers()
{
	FakeRuntime fake = { false, 0 };
	DirectoryCaseRuntime caseRuntime = MakeFakeCaseRuntime(&fake);
	DirectoryEntry root;
	TestCopyString(root.name, sizeof(root.name), "root/");
	root.entryCount = DirectoryEntryNotScanned;
	root.entries = NULL;
	root.backend = NULL;

	stringlist_t results;
	memset(&results, 0, sizeof(results));

	DirectorySearchRuntime searchRuntime = {
		NULL,
		caseRuntime,
		FakeMatchPattern,
		ResultStringCount,
		ResultStringAt,
		ResultAppend
	};
	SearchDirectory(&root, searchRuntime, &results, "folder/*.txt", false);

	const bool passed = results.numstrings == 1 &&
		ExpectString(results.strings[0], "folder/File.TXT");

	for (int i = 0; i < results.numstrings; ++i)
		free(results.strings[i]);
	free(results.strings);
	FreeDirectoryEntries(&root, caseRuntime);
	return passed;
}

int main()
{
	if (!TestSearchPathBackendTypeNames())
	{
		printf("type names failed\n");
		return EXIT_FAILURE;
	}
	if (!TestDirectoryBackendMetadata())
	{
		printf("metadata failed\n");
		return EXIT_FAILURE;
	}
	if (!TestDirectoryBackendPrintInfo())
	{
		printf("print info failed\n");
		return EXIT_FAILURE;
	}
	if (!TestDirectoryBackendDefaultOperations())
	{
		printf("default operations failed\n");
		return EXIT_FAILURE;
	}
	if (!TestDirectoryCaseRepairHelpers())
	{
		printf("case repair failed\n");
		return EXIT_FAILURE;
	}
	if (!TestDirectorySearchHelpers())
	{
		printf("search failed\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
