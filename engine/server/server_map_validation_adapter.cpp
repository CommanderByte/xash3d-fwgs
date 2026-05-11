#include "server_map_validation_adapter.h"

#include "engine/server/server_map_validation.hpp"

namespace
{

sv_map_validation_result_e ToLegacyResult(
	xash::engine::server::ServerMapValidationResult result)
{
	return static_cast<sv_map_validation_result_e>(result);
}

}

extern "C" int SV_MapValidation_MapExists(unsigned int map_flags)
{
	return xash::engine::server::ServerMapExists(map_flags) ? 1 : 0;
}

extern "C" int SV_MapValidation_MapHasLandmark(unsigned int map_flags)
{
	return xash::engine::server::ServerMapHasLandmark(map_flags) ? 1 : 0;
}

extern "C" int SV_MapValidation_MapHasInvalidVersion(unsigned int map_flags)
{
	return xash::engine::server::ServerMapHasInvalidVersion(map_flags) ? 1 : 0;
}

extern "C" int SV_MapValidation_MapExistsForGameDll(unsigned int map_flags)
{
	return xash::engine::server::ServerMapExistsForGameDll(map_flags) ? 1 : 0;
}

extern "C" enum sv_map_validation_result_e SV_MapValidation_Classify(
	unsigned int map_flags)
{
	return ToLegacyResult(
		xash::engine::server::ClassifyServerMapValidation(map_flags));
}

extern "C" sv_changelevel_map_validation_t
SV_MapValidation_BuildChangeLevelDecision(
	unsigned int map_flags,
	int smooth_requested,
	int validate_changelevel)
{
	const xash::engine::server::ServerChangeLevelMapValidationDecision decision =
		xash::engine::server::BuildServerChangeLevelMapValidationDecision(
			map_flags,
			smooth_requested != 0,
			validate_changelevel != 0);

	sv_changelevel_map_validation_t legacy = {};
	legacy.result = ToLegacyResult(decision.result);
	legacy.can_continue = decision.canContinue ? 1 : 0;
	legacy.missing_landmark_for_smooth =
		decision.missingLandmarkForSmooth ? 1 : 0;
	legacy.disable_smooth = decision.disableSmooth ? 1 : 0;
	legacy.exists = decision.flags.exists ? 1 : 0;
	legacy.has_landmark = decision.flags.hasLandmark ? 1 : 0;
	legacy.invalid_version = decision.flags.invalidVersion ? 1 : 0;
	return legacy;
}
