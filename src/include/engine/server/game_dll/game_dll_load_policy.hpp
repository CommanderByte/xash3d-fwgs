#ifndef XASH_ENGINE_SERVER_GAME_DLL_LOAD_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_LOAD_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllRequiredSymbolAction
{
	RejectLibraryLoadFailure,
	RejectMissingEntityApi,
	RejectMissingGiveFnptrs,
	PublishEngineFunctions,
};

enum class GameDllEntityApiAction
{
	UseExtendedEntityApi,
	UseLegacyEntityApi,
	RejectEntityApi,
};

enum class GameDllNewFunctionsAction
{
	NoOptionalExport,
	UseOptionalFunctions,
	ClearOptionalFunctions,
};

enum class GameDllPhysicsApiAction
{
	NoOptionalExport,
	UsePhysicsApi,
	ClearRejectedPhysicsApi,
};

enum class GameDllUnloadAction
{
	SkipNoLibrary,
	UnloadLibraryAndClearState,
};

struct GameDllRequiredSymbolPlan
{
	GameDllRequiredSymbolAction action;
	bool freeLibrary;
	bool freeMempool;
	bool clearLibraryHandle;
	const char *libraryError;
};

struct GameDllApiProbe
{
	bool present;
	bool returnedSuccess;
	int versionAfterCall;
};

struct GameDllEntityApiPlan
{
	GameDllEntityApiAction action;
	bool extendedVersionWarning;
	int observedExtendedVersion;
	int legacyVersionArgument;
	bool freeLibraryOnFailure;
	bool freeMempoolOnFailure;
	bool clearLibraryHandleOnFailure;
};

struct GameDllNewFunctionsPlan
{
	GameDllNewFunctionsAction action;
	bool versionWarning;
	int observedVersion;
	bool clearFunctionTable;
};

struct GameDllPhysicsApiPlan
{
	GameDllPhysicsApiAction action;
	bool initFunctionResult;
	bool loadContinues;
	bool clearFunctionTable;
	bool validateFeaturesFromCallback;
	bool validateZeroFeatures;
};

struct GameDllUnloadPlan
{
	GameDllUnloadAction action;
	bool deactivateServer;
	bool shutdownDelta;
	bool prepareCvarUnlink;
	bool callGameShutdown;
	bool setHostGameloadedFalse;
	bool freeStaticEntities;
	bool killOperatorCommands;
	bool unlinkGameCvars;
	bool unlinkServerDllCommands;
	bool freeStringPool;
	bool resetStudioApi;
	bool freeLibrary;
	bool freeMempool;
	bool clearServerGameState;
};

GameDllRequiredSymbolPlan BuildGameDllRequiredSymbolPlan(
	bool libraryLoaded,
	bool hasGetEntityApi,
	bool hasGetEntityApi2,
	bool hasGiveFnptrsToDll);
GameDllEntityApiPlan BuildGameDllEntityApiPlan(
	GameDllApiProbe extendedApi,
	GameDllApiProbe legacyApi,
	int expectedVersion);
GameDllNewFunctionsPlan BuildGameDllNewFunctionsPlan(
	bool hasExport,
	bool returnedSuccess,
	int versionAfterCall,
	int expectedVersion);
GameDllPhysicsApiPlan BuildGameDllPhysicsApiPlan(
	bool hasExport,
	bool returnedSuccess,
	bool hasFeatureCallback);
GameDllUnloadPlan BuildGameDllUnloadPlan(
	bool libraryLoaded,
	bool hasGameShutdownCallback);

const char *GameDllRequiredSymbolActionName(
	GameDllRequiredSymbolAction action);
const char *GameDllEntityApiActionName(GameDllEntityApiAction action);
const char *GameDllNewFunctionsActionName(GameDllNewFunctionsAction action);
const char *GameDllPhysicsApiActionName(GameDllPhysicsApiAction action);
const char *GameDllUnloadActionName(GameDllUnloadAction action);

}
}
}

#endif
