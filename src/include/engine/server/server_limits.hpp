#ifndef XASH_ENGINE_SERVER_SERVER_LIMITS_HPP
#define XASH_ENGINE_SERVER_SERVER_LIMITS_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerConstraintRole
{
	AbiLayout,
	NetworkProtocol,
	SaveFormat,
	Gameplay,
	PrivateImplementation,
};

struct ServerConstraintDescriptor
{
	const char *name;
	const char *legacyName;
	ServerConstraintRole role;
	bool layoutSensitive;
	bool routeThroughCandidate;
};

constexpr unsigned int kServerHostFlagSkipLocalhost = 1u << 0;
constexpr unsigned int kServerHostFlagMergeVisibility = 1u << 1;

constexpr unsigned int kServerMapExists = 1u << 0;
constexpr unsigned int kServerMapHasLandmark = 1u << 2;
constexpr unsigned int kServerMapInvalidVersion = 1u << 3;

constexpr double kServerSpawnTimeSeconds = 0.1;

constexpr int kServerGroupOpAnd = 0;
constexpr int kServerGroupOpNand = 1;

constexpr int kServerMaxPushedEntities = 256;
constexpr int kServerMaxViewEntities = 128;
constexpr int kServerMaxLocalInfoString = 32768;
constexpr int kServerMaxEntLeafs32 = 24;
constexpr int kServerMaxEntLeafs16 = 48;

constexpr unsigned int kServerClientFlagResendUserinfo = 1u << 0;
constexpr unsigned int kServerClientFlagResendMovevars = 1u << 1;
constexpr unsigned int kServerClientFlagSkipNetMessage = 1u << 2;
constexpr unsigned int kServerClientFlagSendNetMessage = 1u << 3;
constexpr unsigned int kServerClientFlagPredictMovement = 1u << 4;
constexpr unsigned int kServerClientFlagLocalWeapons = 1u << 5;
constexpr unsigned int kServerClientFlagLagCompensation = 1u << 6;
constexpr unsigned int kServerClientFlagFakeClient = 1u << 7;
constexpr unsigned int kServerClientFlagHltvProxy = 1u << 8;
constexpr unsigned int kServerClientFlagSendResources = 1u << 9;
constexpr unsigned int kServerClientFlagForceUnmodified = 1u << 10;

constexpr int kServerChallengeWindowSeconds = 5;

constexpr int kServerMoveNormal = 0;
constexpr int kServerMoveStrafe = 1;
constexpr float kServerMoveEpsilon = 0.01f;
constexpr int kServerMaxClipPlanes = 5;

int ServerEntityLeafCapacity(bool extendedLeafs);
int ServerUpdateMask(int updateBackup);

const ServerConstraintDescriptor *ServerConstraintDescriptors();
std::size_t ServerConstraintDescriptorCount();
const ServerConstraintDescriptor *FindServerConstraintDescriptor(
	const char *legacyName);
const char *ServerConstraintRoleName(ServerConstraintRole role);

}
}
}

#endif
