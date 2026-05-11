#ifndef XASH_ENGINE_SERVER_SERVER_MAP_VALIDATION_HPP
#define XASH_ENGINE_SERVER_SERVER_MAP_VALIDATION_HPP

#include "engine/server/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerMapValidationResult
{
	Valid = 0,
	InvalidVersion = 1,
	Missing = 2,
};

struct ServerMapValidationFlags
{
	bool exists;
	bool hasLandmark;
	bool invalidVersion;
};

struct ServerChangeLevelMapValidationDecision
{
	ServerMapValidationResult result;
	bool canContinue;
	bool missingLandmarkForSmooth;
	bool disableSmooth;
	ServerMapValidationFlags flags;
};

ServerMapValidationFlags DecodeServerMapValidationFlags(unsigned int mapFlags);
bool ServerMapExists(unsigned int mapFlags);
bool ServerMapHasLandmark(unsigned int mapFlags);
bool ServerMapHasInvalidVersion(unsigned int mapFlags);
ServerMapValidationResult ClassifyServerMapValidation(unsigned int mapFlags);
bool ServerMapCanLoad(unsigned int mapFlags);
bool ServerMapExistsForGameDll(unsigned int mapFlags);
ServerChangeLevelMapValidationDecision BuildServerChangeLevelMapValidationDecision(
	unsigned int mapFlags,
	bool smoothRequested,
	bool validateChangeLevel);

}
}
}

#endif
