#include "engine/server/shared/server_map_validation.hpp"

namespace xash
{
namespace engine
{
namespace server
{

ServerMapValidationFlags DecodeServerMapValidationFlags(unsigned int mapFlags)
{
	ServerMapValidationFlags flags = {};
	flags.exists = (mapFlags & kServerMapExists) != 0u;
	flags.hasLandmark = (mapFlags & kServerMapHasLandmark) != 0u;
	flags.invalidVersion = (mapFlags & kServerMapInvalidVersion) != 0u;
	return flags;
}

bool ServerMapExists(unsigned int mapFlags)
{
	return DecodeServerMapValidationFlags(mapFlags).exists;
}

bool ServerMapHasLandmark(unsigned int mapFlags)
{
	return DecodeServerMapValidationFlags(mapFlags).hasLandmark;
}

bool ServerMapHasInvalidVersion(unsigned int mapFlags)
{
	return DecodeServerMapValidationFlags(mapFlags).invalidVersion;
}

ServerMapValidationResult ClassifyServerMapValidation(unsigned int mapFlags)
{
	const ServerMapValidationFlags flags =
		DecodeServerMapValidationFlags(mapFlags);

	if (flags.invalidVersion)
		return ServerMapValidationResult::InvalidVersion;

	if (!flags.exists)
		return ServerMapValidationResult::Missing;

	return ServerMapValidationResult::Valid;
}

bool ServerMapCanLoad(unsigned int mapFlags)
{
	return ClassifyServerMapValidation(mapFlags) ==
		ServerMapValidationResult::Valid;
}

bool ServerMapExistsForGameDll(unsigned int mapFlags)
{
	return ServerMapExists(mapFlags);
}

ServerChangeLevelMapValidationDecision BuildServerChangeLevelMapValidationDecision(
	unsigned int mapFlags,
	bool smoothRequested,
	bool validateChangeLevel)
{
	ServerChangeLevelMapValidationDecision decision = {};
	decision.flags = DecodeServerMapValidationFlags(mapFlags);
	decision.result = ClassifyServerMapValidation(mapFlags);
	decision.canContinue = decision.result == ServerMapValidationResult::Valid;
	decision.missingLandmarkForSmooth =
		decision.canContinue &&
		smoothRequested &&
		!decision.flags.hasLandmark;
	decision.disableSmooth =
		decision.missingLandmarkForSmooth && validateChangeLevel;
	return decision;
}

}
}
}
