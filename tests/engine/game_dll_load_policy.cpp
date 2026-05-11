#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_load_policy.hpp"

using namespace xash::engine::server;

namespace
{

constexpr int kExpectedEntityVersion = 140;
constexpr int kExpectedNewFunctionsVersion = 1;

static bool TestRequiredSymbolPlans()
{
	const GameDllRequiredSymbolPlan libraryFailed =
		BuildGameDllRequiredSymbolPlan(false, true, true, true);
	const GameDllRequiredSymbolPlan missingEntityApi =
		BuildGameDllRequiredSymbolPlan(true, false, false, true);
	const GameDllRequiredSymbolPlan missingGiveFnptrs =
		BuildGameDllRequiredSymbolPlan(true, true, false, false);
	const GameDllRequiredSymbolPlan ready =
		BuildGameDllRequiredSymbolPlan(true, false, true, true);

	return libraryFailed.action ==
			GameDllRequiredSymbolAction::RejectLibraryLoadFailure &&
		!libraryFailed.freeLibrary &&
		libraryFailed.freeMempool &&
		!libraryFailed.clearLibraryHandle &&
		std::strcmp(libraryFailed.libraryError, "library load failed") == 0 &&
		missingEntityApi.action ==
			GameDllRequiredSymbolAction::RejectMissingEntityApi &&
		missingEntityApi.freeLibrary &&
		missingEntityApi.freeMempool &&
		missingEntityApi.clearLibraryHandle &&
		std::strcmp(
			missingEntityApi.libraryError,
			"missing GetEntityAPI and GetEntityAPI2 exports") == 0 &&
		missingGiveFnptrs.action ==
			GameDllRequiredSymbolAction::RejectMissingGiveFnptrs &&
		missingGiveFnptrs.freeLibrary &&
		missingGiveFnptrs.freeMempool &&
		missingGiveFnptrs.clearLibraryHandle &&
		ready.action ==
			GameDllRequiredSymbolAction::PublishEngineFunctions &&
		!ready.freeLibrary &&
		!ready.freeMempool &&
		ready.libraryError == nullptr;
}

static bool TestEntityApiSelection()
{
	const GameDllEntityApiPlan extended =
		BuildGameDllEntityApiPlan(
			{ true, true, kExpectedEntityVersion },
			{ true, true, 0 },
			kExpectedEntityVersion);
	const GameDllEntityApiPlan fallbackAfterMismatch =
		BuildGameDllEntityApiPlan(
			{ true, true, 111 },
			{ true, true, 0 },
			kExpectedEntityVersion);
	const GameDllEntityApiPlan fallbackAfterFailure =
		BuildGameDllEntityApiPlan(
			{ true, false, 109 },
			{ true, true, 0 },
			kExpectedEntityVersion);
	const GameDllEntityApiPlan legacyOnly =
		BuildGameDllEntityApiPlan(
			{ false, false, 0 },
			{ true, true, 0 },
			kExpectedEntityVersion);
	const GameDllEntityApiPlan rejected =
		BuildGameDllEntityApiPlan(
			{ true, false, 109 },
			{ true, false, 0 },
			kExpectedEntityVersion);

	return extended.action == GameDllEntityApiAction::UseExtendedEntityApi &&
		!extended.extendedVersionWarning &&
		extended.legacyVersionArgument == kExpectedEntityVersion &&
		fallbackAfterMismatch.action ==
			GameDllEntityApiAction::UseLegacyEntityApi &&
		fallbackAfterMismatch.extendedVersionWarning &&
		fallbackAfterMismatch.observedExtendedVersion == 111 &&
		fallbackAfterMismatch.legacyVersionArgument == 111 &&
		fallbackAfterFailure.action ==
			GameDllEntityApiAction::UseLegacyEntityApi &&
		!fallbackAfterFailure.extendedVersionWarning &&
		fallbackAfterFailure.legacyVersionArgument == 109 &&
		legacyOnly.action == GameDllEntityApiAction::UseLegacyEntityApi &&
		legacyOnly.legacyVersionArgument == kExpectedEntityVersion &&
		rejected.action == GameDllEntityApiAction::RejectEntityApi &&
		rejected.freeLibraryOnFailure &&
		rejected.freeMempoolOnFailure &&
		rejected.clearLibraryHandleOnFailure;
}

static bool TestOptionalNewFunctionPlans()
{
	const GameDllNewFunctionsPlan missing =
		BuildGameDllNewFunctionsPlan(
			false, false, 0, kExpectedNewFunctionsVersion);
	const GameDllNewFunctionsPlan accepted =
		BuildGameDllNewFunctionsPlan(
			true, true, 99, kExpectedNewFunctionsVersion);
	const GameDllNewFunctionsPlan rejectedSameVersion =
		BuildGameDllNewFunctionsPlan(
			true, false, kExpectedNewFunctionsVersion,
			kExpectedNewFunctionsVersion);
	const GameDllNewFunctionsPlan rejectedMismatch =
		BuildGameDllNewFunctionsPlan(
			true, false, 2, kExpectedNewFunctionsVersion);

	return missing.action == GameDllNewFunctionsAction::NoOptionalExport &&
		!missing.clearFunctionTable &&
		accepted.action == GameDllNewFunctionsAction::UseOptionalFunctions &&
		!accepted.versionWarning &&
		!accepted.clearFunctionTable &&
		rejectedSameVersion.action ==
			GameDllNewFunctionsAction::ClearOptionalFunctions &&
		rejectedSameVersion.clearFunctionTable &&
		!rejectedSameVersion.versionWarning &&
		rejectedMismatch.action ==
			GameDllNewFunctionsAction::ClearOptionalFunctions &&
		rejectedMismatch.clearFunctionTable &&
		rejectedMismatch.versionWarning;
}

static bool TestPhysicsApiPlans()
{
	const GameDllPhysicsApiPlan missing =
		BuildGameDllPhysicsApiPlan(false, false, false);
	const GameDllPhysicsApiPlan acceptedWithFeatures =
		BuildGameDllPhysicsApiPlan(true, true, true);
	const GameDllPhysicsApiPlan acceptedWithoutFeatures =
		BuildGameDllPhysicsApiPlan(true, true, false);
	const GameDllPhysicsApiPlan rejected =
		BuildGameDllPhysicsApiPlan(true, false, true);

	return missing.action == GameDllPhysicsApiAction::NoOptionalExport &&
		missing.initFunctionResult &&
		missing.loadContinues &&
		missing.validateZeroFeatures &&
		acceptedWithFeatures.action ==
			GameDllPhysicsApiAction::UsePhysicsApi &&
		acceptedWithFeatures.validateFeaturesFromCallback &&
		!acceptedWithFeatures.validateZeroFeatures &&
		acceptedWithoutFeatures.action ==
			GameDllPhysicsApiAction::UsePhysicsApi &&
		!acceptedWithoutFeatures.validateFeaturesFromCallback &&
		acceptedWithoutFeatures.validateZeroFeatures &&
		rejected.action ==
			GameDllPhysicsApiAction::ClearRejectedPhysicsApi &&
		!rejected.initFunctionResult &&
		rejected.loadContinues &&
		rejected.clearFunctionTable &&
		rejected.validateZeroFeatures;
}

static bool TestUnloadPlans()
{
	const GameDllUnloadPlan skipped =
		BuildGameDllUnloadPlan(false, true);
	const GameDllUnloadPlan noShutdown =
		BuildGameDllUnloadPlan(true, false);
	const GameDllUnloadPlan withShutdown =
		BuildGameDllUnloadPlan(true, true);

	return skipped.action == GameDllUnloadAction::SkipNoLibrary &&
		!skipped.freeLibrary &&
		noShutdown.action == GameDllUnloadAction::UnloadLibraryAndClearState &&
		noShutdown.deactivateServer &&
		noShutdown.shutdownDelta &&
		noShutdown.prepareCvarUnlink &&
		!noShutdown.callGameShutdown &&
		noShutdown.setHostGameloadedFalse &&
		noShutdown.freeStaticEntities &&
		noShutdown.killOperatorCommands &&
		noShutdown.unlinkGameCvars &&
		noShutdown.unlinkServerDllCommands &&
		noShutdown.freeStringPool &&
		noShutdown.resetStudioApi &&
		noShutdown.freeLibrary &&
		noShutdown.freeMempool &&
		noShutdown.clearServerGameState &&
		withShutdown.callGameShutdown;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllRequiredSymbolActionName(
				GameDllRequiredSymbolAction::PublishEngineFunctions),
			"publish-engine-functions") == 0 &&
		std::strcmp(
			GameDllEntityApiActionName(
				GameDllEntityApiAction::UseLegacyEntityApi),
			"use-legacy-entity-api") == 0 &&
		std::strcmp(
			GameDllNewFunctionsActionName(
				GameDllNewFunctionsAction::ClearOptionalFunctions),
			"clear-optional-functions") == 0 &&
		std::strcmp(
			GameDllPhysicsApiActionName(
				GameDllPhysicsApiAction::ClearRejectedPhysicsApi),
			"clear-rejected-physics-api") == 0 &&
		std::strcmp(
			GameDllUnloadActionName(
				GameDllUnloadAction::UnloadLibraryAndClearState),
			"unload-library-and-clear-state") == 0;
}

}

int main()
{
	if (!TestRequiredSymbolPlans() ||
		!TestEntityApiSelection() ||
		!TestOptionalNewFunctionPlans() ||
		!TestPhysicsApiPlans() ||
		!TestUnloadPlans() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
