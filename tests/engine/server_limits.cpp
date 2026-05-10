#include <cmath>
#include <cstdlib>
#include <cstring>

#include "engine/server/server_limits.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy
{

// Snapshot of legacy values from server.h, edict.h, sv_client.c, sv_move.c,
// and sv_phys.c. The heavy live server header is intentionally not included in
// this pure test because it pulls in model/runtime internals.
constexpr unsigned int kHostFlagSkipLocalhost = 1u << 0;
constexpr unsigned int kHostFlagMergeVisibility = 1u << 1;
constexpr unsigned int kMapExists = 1u << 0;
constexpr unsigned int kMapHasLandmark = 1u << 2;
constexpr unsigned int kMapInvalidVersion = 1u << 3;
constexpr double kSpawnTimeSeconds = 0.1;
constexpr int kGroupOpAnd = 0;
constexpr int kGroupOpNand = 1;
constexpr int kMaxPushedEntities = 256;
constexpr int kMaxViewEntities = 128;
constexpr int kMaxLocalInfoString = 32768;
constexpr int kMaxEntLeafs32 = 24;
constexpr int kMaxEntLeafs16 = 48;
constexpr unsigned int kClientFlagResendUserinfo = 1u << 0;
constexpr unsigned int kClientFlagResendMovevars = 1u << 1;
constexpr unsigned int kClientFlagSkipNetMessage = 1u << 2;
constexpr unsigned int kClientFlagSendNetMessage = 1u << 3;
constexpr unsigned int kClientFlagPredictMovement = 1u << 4;
constexpr unsigned int kClientFlagLocalWeapons = 1u << 5;
constexpr unsigned int kClientFlagLagCompensation = 1u << 6;
constexpr unsigned int kClientFlagFakeClient = 1u << 7;
constexpr unsigned int kClientFlagHltvProxy = 1u << 8;
constexpr unsigned int kClientFlagSendResources = 1u << 9;
constexpr unsigned int kClientFlagForceUnmodified = 1u << 10;

}

static bool NearlyEqual(double left, double right)
{
	return std::fabs(left - right) <= 0.000001;
}

static bool TestPublicLegacyMacroMirrors()
{
	return kServerHostFlagSkipLocalhost == legacy::kHostFlagSkipLocalhost &&
		kServerHostFlagMergeVisibility == legacy::kHostFlagMergeVisibility &&
		kServerMapExists == legacy::kMapExists &&
		kServerMapHasLandmark == legacy::kMapHasLandmark &&
		kServerMapInvalidVersion == legacy::kMapInvalidVersion &&
		NearlyEqual(kServerSpawnTimeSeconds, legacy::kSpawnTimeSeconds) &&
		kServerGroupOpAnd == legacy::kGroupOpAnd &&
		kServerGroupOpNand == legacy::kGroupOpNand &&
		kServerMaxPushedEntities == legacy::kMaxPushedEntities &&
		kServerMaxViewEntities == legacy::kMaxViewEntities &&
		kServerMaxLocalInfoString == legacy::kMaxLocalInfoString &&
		kServerMaxEntLeafs32 == legacy::kMaxEntLeafs32 &&
		kServerMaxEntLeafs16 == legacy::kMaxEntLeafs16 &&
		ServerEntityLeafCapacity(true) == legacy::kMaxEntLeafs32 &&
		ServerEntityLeafCapacity(false) == legacy::kMaxEntLeafs16;
}

static bool TestClientFlagMirrors()
{
	return kServerClientFlagResendUserinfo ==
			legacy::kClientFlagResendUserinfo &&
		kServerClientFlagResendMovevars ==
			legacy::kClientFlagResendMovevars &&
		kServerClientFlagSkipNetMessage ==
			legacy::kClientFlagSkipNetMessage &&
		kServerClientFlagSendNetMessage ==
			legacy::kClientFlagSendNetMessage &&
		kServerClientFlagPredictMovement ==
			legacy::kClientFlagPredictMovement &&
		kServerClientFlagLocalWeapons ==
			legacy::kClientFlagLocalWeapons &&
		kServerClientFlagLagCompensation ==
			legacy::kClientFlagLagCompensation &&
		kServerClientFlagFakeClient == legacy::kClientFlagFakeClient &&
		kServerClientFlagHltvProxy == legacy::kClientFlagHltvProxy &&
		kServerClientFlagSendResources ==
			legacy::kClientFlagSendResources &&
		kServerClientFlagForceUnmodified ==
			legacy::kClientFlagForceUnmodified;
}

static bool TestPrivateLegacyMirrors()
{
	return kServerChallengeWindowSeconds == 5 &&
		kServerMoveNormal == 0 &&
		kServerMoveStrafe == 1 &&
		NearlyEqual(kServerMoveEpsilon, 0.01f) &&
		kServerMaxClipPlanes == 5 &&
		ServerUpdateMask(64) == 63;
}

static bool TestDescriptors()
{
	const ServerConstraintDescriptor *viewEnts =
		FindServerConstraintDescriptor("MAX_VIEWENTS");
	const ServerConstraintDescriptor *challenge =
		FindServerConstraintDescriptor("CHALLENGE_WINDOW_SECONDS");
	const ServerConstraintDescriptor *fakeClient =
		FindServerConstraintDescriptor("FCL_FAKECLIENT");
	const ServerConstraintDescriptor *missing =
		FindServerConstraintDescriptor("NOT_A_SERVER_LIMIT");

	return ServerConstraintDescriptorCount() >= 20 &&
		viewEnts &&
		viewEnts->role == ServerConstraintRole::AbiLayout &&
		viewEnts->layoutSensitive &&
		!viewEnts->routeThroughCandidate &&
		challenge &&
		challenge->role == ServerConstraintRole::NetworkProtocol &&
		!challenge->layoutSensitive &&
		challenge->routeThroughCandidate &&
		fakeClient &&
		fakeClient->role == ServerConstraintRole::PrivateImplementation &&
		!missing;
}

static bool TestRoleNames()
{
	return std::strcmp(
			ServerConstraintRoleName(ServerConstraintRole::AbiLayout),
			"abi-layout") == 0 &&
		std::strcmp(
			ServerConstraintRoleName(ServerConstraintRole::NetworkProtocol),
			"network-protocol") == 0 &&
		std::strcmp(
			ServerConstraintRoleName(ServerConstraintRole::SaveFormat),
			"save-format") == 0 &&
		std::strcmp(
			ServerConstraintRoleName(ServerConstraintRole::Gameplay),
			"gameplay") == 0 &&
		std::strcmp(
			ServerConstraintRoleName(
				ServerConstraintRole::PrivateImplementation),
			"private-implementation") == 0;
}

}

int main()
{
	if (!TestPublicLegacyMacroMirrors() ||
		!TestClientFlagMirrors() ||
		!TestPrivateLegacyMirrors() ||
		!TestDescriptors() ||
		!TestRoleNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
