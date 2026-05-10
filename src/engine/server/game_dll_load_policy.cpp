#include "engine/server/game_dll_load_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

GameDllRequiredSymbolPlan RejectRequiredSymbol(
	GameDllRequiredSymbolAction action,
	const char *libraryError,
	bool freeLibrary)
{
	GameDllRequiredSymbolPlan plan = {};
	plan.action = action;
	plan.freeLibrary = freeLibrary;
	plan.freeMempool = true;
	plan.clearLibraryHandle = freeLibrary;
	plan.libraryError = libraryError;
	return plan;
}

}

GameDllRequiredSymbolPlan BuildGameDllRequiredSymbolPlan(
	bool libraryLoaded,
	bool hasGetEntityApi,
	bool hasGetEntityApi2,
	bool hasGiveFnptrsToDll)
{
	if (!libraryLoaded)
	{
		return RejectRequiredSymbol(
			GameDllRequiredSymbolAction::RejectLibraryLoadFailure,
			"library load failed",
			false);
	}

	if (!hasGetEntityApi && !hasGetEntityApi2)
	{
		return RejectRequiredSymbol(
			GameDllRequiredSymbolAction::RejectMissingEntityApi,
			"missing GetEntityAPI and GetEntityAPI2 exports",
			true);
	}

	if (!hasGiveFnptrsToDll)
	{
		return RejectRequiredSymbol(
			GameDllRequiredSymbolAction::RejectMissingGiveFnptrs,
			"missing GiveFnptrsToDll export",
			true);
	}

	GameDllRequiredSymbolPlan plan = {};
	plan.action = GameDllRequiredSymbolAction::PublishEngineFunctions;
	return plan;
}

GameDllEntityApiPlan BuildGameDllEntityApiPlan(
	GameDllApiProbe extendedApi,
	GameDllApiProbe legacyApi,
	int expectedVersion)
{
	GameDllEntityApiPlan plan = {};
	plan.legacyVersionArgument = expectedVersion;

	if (extendedApi.present)
	{
		plan.observedExtendedVersion = extendedApi.versionAfterCall;
		plan.legacyVersionArgument = extendedApi.versionAfterCall;

		if (extendedApi.returnedSuccess)
		{
			if (extendedApi.versionAfterCall == expectedVersion)
			{
				plan.action = GameDllEntityApiAction::UseExtendedEntityApi;
				return plan;
			}

			plan.extendedVersionWarning = true;
		}
	}

	if (legacyApi.present && legacyApi.returnedSuccess)
	{
		plan.action = GameDllEntityApiAction::UseLegacyEntityApi;
		return plan;
	}

	plan.action = GameDllEntityApiAction::RejectEntityApi;
	plan.freeLibraryOnFailure = true;
	plan.freeMempoolOnFailure = true;
	plan.clearLibraryHandleOnFailure = true;
	return plan;
}

GameDllNewFunctionsPlan BuildGameDllNewFunctionsPlan(
	bool hasExport,
	bool returnedSuccess,
	int versionAfterCall,
	int expectedVersion)
{
	GameDllNewFunctionsPlan plan = {};
	plan.observedVersion = versionAfterCall;

	if (!hasExport)
	{
		plan.action = GameDllNewFunctionsAction::NoOptionalExport;
		return plan;
	}

	if (returnedSuccess)
	{
		plan.action = GameDllNewFunctionsAction::UseOptionalFunctions;
		return plan;
	}

	plan.action = GameDllNewFunctionsAction::ClearOptionalFunctions;
	plan.clearFunctionTable = true;
	plan.versionWarning = versionAfterCall != expectedVersion;
	return plan;
}

GameDllPhysicsApiPlan BuildGameDllPhysicsApiPlan(
	bool hasExport,
	bool returnedSuccess,
	bool hasFeatureCallback)
{
	GameDllPhysicsApiPlan plan = {};
	plan.loadContinues = true;

	if (!hasExport)
	{
		plan.action = GameDllPhysicsApiAction::NoOptionalExport;
		plan.initFunctionResult = true;
		plan.validateZeroFeatures = true;
		return plan;
	}

	if (returnedSuccess)
	{
		plan.action = GameDllPhysicsApiAction::UsePhysicsApi;
		plan.initFunctionResult = true;
		plan.validateFeaturesFromCallback = hasFeatureCallback;
		plan.validateZeroFeatures = !hasFeatureCallback;
		return plan;
	}

	plan.action = GameDllPhysicsApiAction::ClearRejectedPhysicsApi;
	plan.initFunctionResult = false;
	plan.clearFunctionTable = true;
	plan.validateZeroFeatures = true;
	return plan;
}

GameDllUnloadPlan BuildGameDllUnloadPlan(
	bool libraryLoaded,
	bool hasGameShutdownCallback)
{
	GameDllUnloadPlan plan = {};

	if (!libraryLoaded)
	{
		plan.action = GameDllUnloadAction::SkipNoLibrary;
		return plan;
	}

	plan.action = GameDllUnloadAction::UnloadLibraryAndClearState;
	plan.deactivateServer = true;
	plan.shutdownDelta = true;
	plan.prepareCvarUnlink = true;
	plan.callGameShutdown = hasGameShutdownCallback;
	plan.setHostGameloadedFalse = true;
	plan.freeStaticEntities = true;
	plan.killOperatorCommands = true;
	plan.unlinkGameCvars = true;
	plan.unlinkServerDllCommands = true;
	plan.freeStringPool = true;
	plan.resetStudioApi = true;
	plan.freeLibrary = true;
	plan.freeMempool = true;
	plan.clearServerGameState = true;
	return plan;
}

const char *GameDllRequiredSymbolActionName(
	GameDllRequiredSymbolAction action)
{
	switch (action)
	{
	case GameDllRequiredSymbolAction::RejectLibraryLoadFailure:
		return "reject-library-load-failure";
	case GameDllRequiredSymbolAction::RejectMissingEntityApi:
		return "reject-missing-entity-api";
	case GameDllRequiredSymbolAction::RejectMissingGiveFnptrs:
		return "reject-missing-give-fnptrs";
	case GameDllRequiredSymbolAction::PublishEngineFunctions:
		return "publish-engine-functions";
	}

	return "unknown";
}

const char *GameDllEntityApiActionName(GameDllEntityApiAction action)
{
	switch (action)
	{
	case GameDllEntityApiAction::UseExtendedEntityApi:
		return "use-extended-entity-api";
	case GameDllEntityApiAction::UseLegacyEntityApi:
		return "use-legacy-entity-api";
	case GameDllEntityApiAction::RejectEntityApi:
		return "reject-entity-api";
	}

	return "unknown";
}

const char *GameDllNewFunctionsActionName(GameDllNewFunctionsAction action)
{
	switch (action)
	{
	case GameDllNewFunctionsAction::NoOptionalExport:
		return "no-optional-export";
	case GameDllNewFunctionsAction::UseOptionalFunctions:
		return "use-optional-functions";
	case GameDllNewFunctionsAction::ClearOptionalFunctions:
		return "clear-optional-functions";
	}

	return "unknown";
}

const char *GameDllPhysicsApiActionName(GameDllPhysicsApiAction action)
{
	switch (action)
	{
	case GameDllPhysicsApiAction::NoOptionalExport:
		return "no-optional-export";
	case GameDllPhysicsApiAction::UsePhysicsApi:
		return "use-physics-api";
	case GameDllPhysicsApiAction::ClearRejectedPhysicsApi:
		return "clear-rejected-physics-api";
	}

	return "unknown";
}

const char *GameDllUnloadActionName(GameDllUnloadAction action)
{
	switch (action)
	{
	case GameDllUnloadAction::SkipNoLibrary:
		return "skip-no-library";
	case GameDllUnloadAction::UnloadLibraryAndClearState:
		return "unload-library-and-clear-state";
	}

	return "unknown";
}

}
}
}
