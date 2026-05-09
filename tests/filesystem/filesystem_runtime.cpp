#include <stdlib.h>
#include <string.h>

#include "filesystem/filesystem_runtime.hpp"

using namespace xash::filesystem;

struct TestPath
{
	TestPath *next;
	bool isStatic;
	int closeCount;
	int freeCount;
};

struct TestListState
{
	int markGamesCalls;
};

static searchpath_t *AsSearchPath(TestPath *path)
{
	return reinterpret_cast<searchpath_t *>(path);
}

static TestPath *AsTestPath(searchpath_t *path)
{
	return reinterpret_cast<TestPath *>(path);
}

static searchpath_t *TestNext(void *, searchpath_t *path)
{
	return AsSearchPath(AsTestPath(path)->next);
}

static void TestSetNext(void *, searchpath_t *path, searchpath_t *next)
{
	AsTestPath(path)->next = AsTestPath(next);
}

static bool TestIsStatic(void *, searchpath_t *path)
{
	return AsTestPath(path)->isStatic;
}

static void TestClose(void *, searchpath_t *path)
{
	AsTestPath(path)->closeCount++;
}

static void TestFree(void *, searchpath_t *path)
{
	AsTestPath(path)->freeCount++;
}

static void TestMarkGames(void *context)
{
	static_cast<TestListState *>(context)->markGamesCalls++;
}

static SearchPathListOps MakeOps(TestListState *state)
{
	SearchPathListOps ops = {
		state,
		TestNext,
		TestSetNext,
		TestIsStatic,
		TestClose,
		TestFree,
		TestMarkGames
	};
	return ops;
}

static bool TestRuntimeOwnsState()
{
	FilesystemRuntime runtime;
	FilesystemStateConfig config = {
		"root",
		"base",
		"game",
		"ro",
		"english",
		NULL,
		NULL,
		true
	};
	runtime.configure(config);

	return strcmp(runtime.state().rootDir(), "root") == 0 &&
		strcmp(runtime.state().baseDir(), "base") == 0 &&
		strcmp(runtime.state().gameDir(), "game") == 0 &&
		strcmp(runtime.state().readOnlyDir(), "ro") == 0 &&
		strcmp(runtime.state().language(), "english") == 0 &&
		runtime.state().directPathsEnabled();
}

static bool TestPrependAndClearDynamicSearchPaths()
{
	FilesystemRuntime runtime;
	TestListState state = {};
	SearchPathListOps ops = MakeOps(&state);
	TestPath dynamicA = {};
	TestPath staticB = {};
	TestPath dynamicC = {};
	staticB.isStatic = true;

	runtime.prependSearchPath(AsSearchPath(&dynamicC), ops);
	runtime.prependSearchPath(AsSearchPath(&staticB), ops);
	runtime.prependSearchPath(AsSearchPath(&dynamicA), ops);
	runtime.setWritePath(AsSearchPath(&staticB));

	SearchPathClearResult result = runtime.clearDynamicSearchPaths(ops);

	return result.searchPaths == AsSearchPath(&staticB) &&
		result.writePath == AsSearchPath(&staticB) &&
		staticB.next == NULL &&
		result.keptCount == 1 &&
		result.removedCount == 2 &&
		dynamicA.closeCount == 1 &&
		dynamicA.freeCount == 1 &&
		dynamicC.closeCount == 1 &&
		dynamicC.freeCount == 1 &&
		staticB.closeCount == 0 &&
		staticB.freeCount == 0 &&
		state.markGamesCalls == 1;
}

static bool TestClearDropsUnmountedWritePath()
{
	FilesystemRuntime runtime;
	TestListState state = {};
	SearchPathListOps ops = MakeOps(&state);
	TestPath dynamicA = {};
	TestPath dynamicB = {};

	runtime.prependSearchPath(AsSearchPath(&dynamicB), ops);
	runtime.prependSearchPath(AsSearchPath(&dynamicA), ops);
	runtime.setWritePath(AsSearchPath(&dynamicB));

	SearchPathClearResult result = runtime.clearDynamicSearchPaths(ops);

	return result.searchPaths == NULL &&
		result.writePath == NULL &&
		runtime.writePath() == NULL &&
		result.removedCount == 2;
}

static bool TestMissingSearchPathCallbacksAreNoop()
{
	FilesystemRuntime runtime;
	SearchPathListOps ops = {};
	TestPath path = {};

	runtime.prependSearchPath(AsSearchPath(&path), ops);
	SearchPathClearResult result = runtime.clearDynamicSearchPaths(ops);

	return runtime.searchPaths() == NULL &&
		result.searchPaths == NULL &&
		result.removedCount == 0;
}

struct FileMemoryState
{
	int allocCalls;
	int freeCalls;
	file_t *file;
};

static file_t *TestAllocFile(void *context, bool clear)
{
	FileMemoryState *state = static_cast<FileMemoryState *>(context);
	state->allocCalls++;
	return clear ? state->file : NULL;
}

static void TestFreeFile(void *context, file_t *file)
{
	FileMemoryState *state = static_cast<FileMemoryState *>(context);
	if (file == state->file)
		state->freeCalls++;
}

static bool TestFileHandleMemory()
{
	FilesystemRuntime runtime;
	FileMemoryState state = {};
	state.file = reinterpret_cast<file_t *>(&state);
	FileHandleMemoryOps ops = {
		&state,
		TestAllocFile,
		TestFreeFile
	};

	file_t *file = runtime.allocateFile(ops);
	runtime.freeFile(ops, file);

	return file == state.file &&
		state.allocCalls == 1 &&
		state.freeCalls == 1;
}

static bool TestBeginRescanPlan()
{
	FilesystemRuntime runtime;
	runtime.state().setDirectPathsEnabled(true);
	FilesystemRescanPlan plan = runtime.beginRescan(0x0f, "french", 0x07, 0x04);

	if (plan.mountFlags != 0x07 ||
		!plan.localizationEnabled ||
		strcmp(plan.language, "french") != 0 ||
		runtime.state().directPathsEnabled())
	{
		return false;
	}

	plan = runtime.beginRescan(0x03, "german", 0x07, 0x04);

	return plan.mountFlags == 0x03 &&
		!plan.localizationEnabled &&
		strcmp(plan.language, "") == 0;
}

int main()
{
	if (!TestRuntimeOwnsState() ||
		!TestPrependAndClearDynamicSearchPaths() ||
		!TestClearDropsUnmountedWritePath() ||
		!TestMissingSearchPathCallbacksAreNoop() ||
		!TestFileHandleMemory() ||
		!TestBeginRescanPlan())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
