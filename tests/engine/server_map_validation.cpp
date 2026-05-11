#include <cstdlib>

#include "engine/server/server_map_validation.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy
{

constexpr unsigned int kMapExists = 1u << 0;
constexpr unsigned int kMapHasLandmark = 1u << 2;
constexpr unsigned int kMapInvalidVersion = 1u << 3;

}

static bool TestFlagDecoding()
{
	const ServerMapValidationFlags empty = DecodeServerMapValidationFlags(0u);
	const ServerMapValidationFlags all = DecodeServerMapValidationFlags(
		legacy::kMapExists |
		legacy::kMapHasLandmark |
		legacy::kMapInvalidVersion);

	return !empty.exists &&
		!empty.hasLandmark &&
		!empty.invalidVersion &&
		all.exists &&
		all.hasLandmark &&
		all.invalidVersion &&
		ServerMapExists(legacy::kMapExists) &&
		ServerMapHasLandmark(legacy::kMapHasLandmark) &&
		ServerMapHasInvalidVersion(legacy::kMapInvalidVersion);
}

static bool TestLoadClassification()
{
	return ClassifyServerMapValidation(legacy::kMapExists) ==
			ServerMapValidationResult::Valid &&
		ClassifyServerMapValidation(0u) ==
			ServerMapValidationResult::Missing &&
		ClassifyServerMapValidation(legacy::kMapInvalidVersion) ==
			ServerMapValidationResult::InvalidVersion &&
		ClassifyServerMapValidation(
			legacy::kMapExists | legacy::kMapInvalidVersion) ==
			ServerMapValidationResult::InvalidVersion &&
		ServerMapCanLoad(legacy::kMapExists) &&
		!ServerMapCanLoad(0u) &&
		!ServerMapCanLoad(
			legacy::kMapExists | legacy::kMapInvalidVersion);
}

static bool TestGameDllMapExistsCompatibility()
{
	return ServerMapExistsForGameDll(legacy::kMapExists) &&
		ServerMapExistsForGameDll(
			legacy::kMapExists | legacy::kMapInvalidVersion) &&
		!ServerMapExistsForGameDll(0u);
}

static bool TestChangeLevelInvalidAndMissing()
{
	const ServerChangeLevelMapValidationDecision invalid =
		BuildServerChangeLevelMapValidationDecision(
			legacy::kMapExists | legacy::kMapInvalidVersion,
			true,
			true);
	const ServerChangeLevelMapValidationDecision missing =
		BuildServerChangeLevelMapValidationDecision(0u, true, true);

	return invalid.result == ServerMapValidationResult::InvalidVersion &&
		!invalid.canContinue &&
		!invalid.missingLandmarkForSmooth &&
		!invalid.disableSmooth &&
		missing.result == ServerMapValidationResult::Missing &&
		!missing.canContinue &&
		!missing.missingLandmarkForSmooth &&
		!missing.disableSmooth;
}

static bool TestChangeLevelLandmarkPolicy()
{
	const ServerChangeLevelMapValidationDecision classic =
		BuildServerChangeLevelMapValidationDecision(
			legacy::kMapExists,
			false,
			true);
	const ServerChangeLevelMapValidationDecision smoothWithoutValidation =
		BuildServerChangeLevelMapValidationDecision(
			legacy::kMapExists,
			true,
			false);
	const ServerChangeLevelMapValidationDecision smoothWithValidation =
		BuildServerChangeLevelMapValidationDecision(
			legacy::kMapExists,
			true,
			true);
	const ServerChangeLevelMapValidationDecision smoothWithLandmark =
		BuildServerChangeLevelMapValidationDecision(
			legacy::kMapExists | legacy::kMapHasLandmark,
			true,
			true);

	return classic.canContinue &&
		!classic.missingLandmarkForSmooth &&
		!classic.disableSmooth &&
		smoothWithoutValidation.canContinue &&
		smoothWithoutValidation.missingLandmarkForSmooth &&
		!smoothWithoutValidation.disableSmooth &&
		smoothWithValidation.canContinue &&
		smoothWithValidation.missingLandmarkForSmooth &&
		smoothWithValidation.disableSmooth &&
		smoothWithLandmark.canContinue &&
		!smoothWithLandmark.missingLandmarkForSmooth &&
		!smoothWithLandmark.disableSmooth;
}

}

int main()
{
	if (!TestFlagDecoding() ||
		!TestLoadClassification() ||
		!TestGameDllMapExistsCompatibility() ||
		!TestChangeLevelInvalidAndMissing() ||
		!TestChangeLevelLandmarkPolicy())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
