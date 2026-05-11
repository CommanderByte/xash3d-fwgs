#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "engine/server/client/client_command_dispatch.hpp"
#include "engine/server/client/client_policy.hpp"
#include "engine/server/client/client_session_slots.hpp"
#include "engine/server/client/connection_response.hpp"
#include "engine/server/client/connectionless_classifier.hpp"
#include "engine/server/client/remote_admin_command.hpp"
#include "engine/server/client/server_challenge_policy.hpp"
#include "engine/server/client/server_timeout_policy.hpp"
#include "engine/server/client/user_agent_policy.hpp"
#include "engine/server/netapi_info.hpp"
#include "engine/server/server_limits.hpp"
#include "engine/server/source_query.hpp"

using namespace xash::engine::server;

namespace
{

bool CloseEnough(double lhs, double rhs)
{
	return std::fabs(lhs - rhs) < 0.000001;
}

bool Equals(const char *lhs, const char *rhs)
{
	return std::strcmp(lhs, rhs) == 0;
}

const char *ValidUuid()
{
	return "0123456789abcdef0123456789abcdef";
}

UserAgentPolicy AllowAllUserAgents()
{
	UserAgentPolicy policy = {};
	policy.allowNoInputDevices = true;
	policy.allowTouch = true;
	policy.allowMouse = true;
	policy.allowJoystick = true;
	policy.allowVr = true;
	return policy;
}

ClientCommandDecision ClassifyClientCommandName(
	const char *name,
	bool spawned = true,
	bool entTools = true,
	bool fullUpdateThrottled = false)
{
	ClientCommandContext context = {};
	context.commandName = name;
	context.serverActive = true;
	context.clientSpawned = spawned;
	context.entToolsEnabled = entTools;
	context.serverBackground = false;
	context.fullUpdateThrottled = fullUpdateThrottled;
	return ClassifyClientCommand(context);
}

bool TestConnectionAdmissionHelpersCompose()
{
	char challenge[64] = {};
	char reject[128] = {};
	char rcon[64] = {};

	const ServerChallengeWindowPair windows =
		BuildServerChallengeAcceptedWindows(12.0);
	std::size_t used = ResetRemoteAdminCommand(rcon, sizeof(rcon));
	used = AppendRemoteAdminCommandArgument(
		rcon,
		sizeof(rcon),
		used,
		"status");

	UserAgentPolicy policy = AllowAllUserAgents();
	const UserAgentValidationCode accepted =
		ValidateUserAgent(ValidUuid(), "3", policy);
	policy.bannedId = true;

	return ClassifyServerConnectionlessCommand(
			"getchallenge",
			"getchallenge",
			true,
			false) == ServerConnectionlessCommand::ChallengeRequest &&
		windows.current == 2u &&
		windows.previous == 1u &&
		ServerChallengeAcceptsWindow(1u, windows) &&
		!ServerChallengeAcceptsWindow(0u, windows) &&
		FormatChallengeResponse(challenge, sizeof(challenge), 12345, true) &&
		Equals(challenge, "challenge 12345 0") &&
		FormatRejectErrorMessage(reject, sizeof(reject), "server is full\n") &&
		std::strstr(reject, "server is full") != nullptr &&
		accepted == UserAgentValidationCode::Accepted &&
		ValidateUserAgent(ValidUuid(), "3", policy) ==
			UserAgentValidationCode::BannedId &&
		Equals(
			UserAgentRejectionMessage(UserAgentValidationCode::BannedId),
			"You are banned!\n") &&
		BuildRemoteAdminAuthAction(true, "secret", "secret") ==
			RemoteAdminAuthAction::Accept &&
		used == std::strlen("\"status\" ") &&
		Equals(rcon, "\"status\" ");
}

bool TestSessionUserinfoCommandAndTimeoutHelpersCompose()
{
	const ClientSessionSlotSnapshot slots[] = {
		{ kClientSessionSlotFree, 0u },
		{ kClientSessionSlotConnected, 0u },
		{ kClientSessionSlotSpawning, kServerClientFlagFakeClient },
		{ kClientSessionSlotSpawned, 0u },
	};

	const ClientSessionPopulation population =
		CountClientSessionPopulation(slots, 4);
	const unsigned int flags =
		kServerClientFlagResendUserinfo |
		kServerClientFlagPredictMovement |
		kServerClientFlagLocalWeapons;
	const ClientFlagSnapshot snapshot = BuildClientFlagSnapshot(flags);

	UserinfoPenaltyInput penalty = {};
	penalty.penaltyEnabled = true;
	penalty.realtime = 10.0;
	penalty.nextChangeTime = 0.0;
	penalty.penalty = 0.0;
	penalty.basePenalty = 1.0;
	penalty.penaltyMultiplier = 2.0;
	penalty.maxAttempts = 3;

	ServerTimeoutClientRequest timeout = {};
	timeout.state = kClientSessionSlotSpawned;
	timeout.hasEntity = true;
	timeout.lastReceived = 5.0;
	timeout.connectionStarted = 20.0;
	timeout.connectedDropPoint = 10.0;
	timeout.spawnedDropPoint = 10.0;

	const ClientCommandDecision begin = ClassifyClientCommandName("begin");
	const ClientCommandDecision entFire =
		ClassifyClientCommandName("ent_fire");
	const ClientCommandDecision entFireWithoutTools =
		ClassifyClientCommandName("ent_fire", true, false);
	const ClientCommandDecision fullUpdate =
		ClassifyClientCommandName("fullupdate");
	const ClientCommandDecision throttledFullUpdate =
		ClassifyClientCommandName("fullupdate", true, true, true);
	const UserinfoPenaltyPlan penaltyPlan =
		BuildUserinfoPenaltyPlan(penalty);
	const ServerTimeoutClientPlan timeoutPlan =
		BuildServerTimeoutClientPlan(timeout);

	return population.connected == 3 &&
		population.players == 2 &&
		population.bots == 1 &&
		FindFirstFreeClientSessionSlot(slots, 4) == 0 &&
		BuildClientSessionConnectMasterUpdate(1, 8) ==
			ClientSessionMasterUpdate::FirstConnectedClient &&
		BuildClientSessionDropMasterUpdate(0) ==
			ClientSessionMasterUpdate::EmptyServer &&
		snapshot.resendUserinfo &&
		snapshot.predictMovement &&
		snapshot.localWeapons &&
		!snapshot.fakeClient &&
		penaltyPlan.allowUpdate &&
		CloseEnough(penaltyPlan.penalty, 1.0) &&
		CloseEnough(
			ResolveRequestedClientRate(0, 5000.0, 1000.0, 100000.0),
			5000.0) &&
		CloseEnough(
			ApplyUpdateIntervalLimits(
				ResolveRequestedUpdateInterval(100, 20),
				50.0,
				0.0),
			0.02) &&
		begin.route == ClientCommandRoute::Builtin &&
		begin.commandIndex >= 0 &&
		entFire.route == ClientCommandRoute::EntTools &&
		entFireWithoutTools.route == ClientCommandRoute::GameDll &&
		fullUpdate.route == ClientCommandRoute::FullUpdate &&
		throttledFullUpdate.route == ClientCommandRoute::Ignore &&
		timeoutPlan.activePlayer &&
		timeoutPlan.action == ServerTimeoutClientAction::DropSpawned &&
		!timeoutPlan.ban &&
		ShouldReleaseServerPauseForTimeouts(2, true, 0);
}

bool TestQueryResponseHelpersCompose()
{
	unsigned char sourceBuffer[256] = {};
	SourceQueryDetails source = {};
	source.protocolVersion = 49;
	source.hostname = "Test Host";
	source.mapName = "crossfire";
	source.gameFolder = "valve";
	source.gameDescription = "Half-Life";
	source.appId = 70;
	source.playerCount = 3;
	source.maxPlayers = 8;
	source.botCount = 1;
	source.serverType = 'd';
	source.platform = 'w';
	source.passwordProtected = true;
	source.secure = 0;
	source.version = "0.21";

	const std::size_t sourceBytes =
		BuildSourceQueryDetails(source, sourceBuffer, sizeof(sourceBuffer));

	char netapi[512] = {};
	LegacyServerInfo legacy = {};
	legacy.requestProtocol = 49;
	legacy.protocolVersion = 49;
	legacy.hostname = "Test Host";
	legacy.mapName = "crossfire";
	legacy.deathmatch = true;
	legacy.teamplay = false;
	legacy.coop = false;
	legacy.playerCount = 3;
	legacy.maxPlayers = 8;
	legacy.gameFolder = "valve";
	legacy.passwordProtected = true;

	return sourceBytes > 8 &&
		sourceBuffer[0] == 0xff &&
		sourceBuffer[1] == 0xff &&
		sourceBuffer[2] == 0xff &&
		sourceBuffer[3] == 0xff &&
		sourceBuffer[4] == kSourceQueryInfoResponse &&
		SourceQueryAllowsPlayerList(true, false) &&
		!SourceQueryAllowsPlayerList(true, true) &&
		BuildLegacyServerInfoString(netapi, sizeof(netapi), legacy) &&
		std::strstr(netapi, "\\map\\crossfire") != nullptr &&
		std::strstr(netapi, "\\numcl\\3") != nullptr &&
		std::strstr(netapi, "\\password\\1") != nullptr;
}

}

int main()
{
	if (!TestConnectionAdmissionHelpersCompose() ||
		!TestSessionUserinfoCommandAndTimeoutHelpersCompose() ||
		!TestQueryResponseHelpersCompose())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
