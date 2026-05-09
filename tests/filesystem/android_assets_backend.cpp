#include <stdlib.h>
#include <string.h>

#include "filesystem/android_assets_backend.hpp"

using namespace xash::filesystem;

struct HookState
{
	int closeCalls;
	int printCalls;
	int openCalls;
	int fileTimeCalls;
	int findCalls;
	int searchCalls;
	int loadCalls;
	bool sawCaseInsensitive;
};

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"org.example.game",
		SearchPathBackendType::AndroidAssets,
		16,
		5,
		"android assets"
	};
	return metadata;
}

static void HookClose(void *context)
{
	static_cast<HookState *>(context)->closeCalls++;
}

static void HookPrintInfo(void *context, char *dst, size_t size)
{
	static_cast<HookState *>(context)->printCalls++;

	if (!dst || size == 0)
		return;

	const char *text = "android package";
	size_t i = 0;
	for (; i + 1 < size && text[i]; ++i)
		dst[i] = text[i];
	dst[i] = '\0';
}

static file_t *HookOpenFile(void *context, const char *path, const char *mode,
	int index)
{
	static_cast<HookState *>(context)->openCalls++;

	if (!ExpectString(path, "asset.txt") || !ExpectString(mode, "rb") ||
		index != 7)
	{
		return NULL;
	}

	return reinterpret_cast<file_t *>(context);
}

static int HookFileTime(void *context, const char *path)
{
	static_cast<HookState *>(context)->fileTimeCalls++;

	return ExpectString(path, "asset.txt") ? 123 : -1;
}

static int HookFindFile(void *context, const char *path, char *fixedName,
	size_t len)
{
	static_cast<HookState *>(context)->findCalls++;

	if (!ExpectString(path, "asset.txt"))
		return -1;

	if (fixedName && len >= 10)
		strcpy(fixedName, "asset.txt");

	return 7;
}

static void HookSearch(void *context, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	(void)list;

	HookState *state = static_cast<HookState *>(context);
	state->searchCalls++;
	state->sawCaseInsensitive = caseInsensitive &&
		ExpectString(pattern, "*.txt");
}

static byte *HookLoadFile(void *context, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	(void)freeFn;

	static_cast<HookState *>(context)->loadCalls++;

	if (!ExpectString(path, "asset.txt") || index != 7 || !alloc)
		return NULL;

	byte *data = static_cast<byte *>(alloc(2));
	data[0] = 'x';
	data[1] = '\0';

	if (fileSize)
		*fileSize = 1;

	return data;
}

static void *TestAlloc(size_t size)
{
	return malloc(size);
}

static void TestFree(void *ptr)
{
	free(ptr);
}

static bool TestAndroidAssetsBackendMetadata()
{
	AndroidAssetsBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "org.example.game") &&
		metadata.type == SearchPathBackendType::AndroidAssets &&
		metadata.flags == 16 &&
		metadata.order == 5 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "android assets");
}

static bool TestAndroidAssetsBackendPrintInfo()
{
	AndroidAssetsBackend backend(TestMetadata());
	char output[8];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "org.exa"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestAndroidAssetsBackendDefaultOperations()
{
	AndroidAssetsBackend backend(TestMetadata());

	if (backend.openFile("asset.txt", "rb", 0))
		return false;

	if (backend.fileTime("asset.txt") != -1)
		return false;

	if (backend.findFile("asset.txt", NULL, 0) != -1)
		return false;

	if (backend.loadFile("asset.txt", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

static bool TestAndroidAssetsBackendHooks()
{
	HookState state = {};
	AndroidAssetsBackendOps ops = {
		&state,
		HookClose,
		HookPrintInfo,
		HookOpenFile,
		HookFileTime,
		HookFindFile,
		HookSearch,
		HookLoadFile
	};
	AndroidAssetsBackend backend(TestMetadata(), ops);

	char output[32];
	backend.printInfo(output, sizeof(output));
	if (!ExpectString(output, "android package"))
		return false;

	if (backend.openFile("asset.txt", "rb", 7) !=
		reinterpret_cast<file_t *>(&state))
	{
		return false;
	}

	if (backend.fileTime("asset.txt") != 123)
		return false;

	char fixedName[16] = {};
	if (backend.findFile("asset.txt", fixedName, sizeof(fixedName)) != 7 ||
		!ExpectString(fixedName, "asset.txt"))
	{
		return false;
	}

	backend.search(NULL, "*.txt", true);
	if (!state.sawCaseInsensitive)
		return false;

	fs_offset_t fileSize = 0;
	byte *data = backend.loadFile("asset.txt", 7, &fileSize, TestAlloc,
		TestFree);
	if (!data || fileSize != 1 || data[0] != 'x')
		return false;
	TestFree(data);

	backend.close();

	return state.closeCalls == 1 &&
		state.printCalls == 1 &&
		state.openCalls == 1 &&
		state.fileTimeCalls == 1 &&
		state.findCalls == 1 &&
		state.searchCalls == 1 &&
		state.loadCalls == 1;
}

int main()
{
	if (!TestAndroidAssetsBackendMetadata() ||
		!TestAndroidAssetsBackendPrintInfo() ||
		!TestAndroidAssetsBackendDefaultOperations() ||
		!TestAndroidAssetsBackendHooks())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
