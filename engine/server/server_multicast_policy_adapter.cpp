#include "server_multicast_policy_adapter.h"

#include "const.h"
#include "engine/server/messaging/server_multicast_policy.hpp"

static_assert(MSG_BROADCAST == xash::engine::server::kMulticastDestinationBroadcast,
	"MSG_BROADCAST value changed");
static_assert(MSG_ONE == xash::engine::server::kMulticastDestinationOne,
	"MSG_ONE value changed");
static_assert(MSG_ALL == xash::engine::server::kMulticastDestinationAll,
	"MSG_ALL value changed");
static_assert(MSG_INIT == xash::engine::server::kMulticastDestinationInit,
	"MSG_INIT value changed");
static_assert(MSG_PVS == xash::engine::server::kMulticastDestinationPvs,
	"MSG_PVS value changed");
static_assert(MSG_PAS == xash::engine::server::kMulticastDestinationPas,
	"MSG_PAS value changed");
static_assert(MSG_PVS_R == xash::engine::server::kMulticastDestinationPvsReliable,
	"MSG_PVS_R value changed");
static_assert(MSG_PAS_R == xash::engine::server::kMulticastDestinationPasReliable,
	"MSG_PAS_R value changed");
static_assert(MSG_ONE_UNRELIABLE == xash::engine::server::kMulticastDestinationOneUnreliable,
	"MSG_ONE_UNRELIABLE value changed");
static_assert(MSG_SPEC == xash::engine::server::kMulticastDestinationSpectator,
	"MSG_SPEC value changed");

extern "C" sv_multicast_destination_plan_t SV_Multicast_BuildDestinationPlan(
	int destination,
	int server_loading,
	int origin_provided)
{
	xash::engine::server::MulticastDestinationRequest request = {};
	request.destination = destination;
	request.serverLoading = server_loading != 0;
	request.originProvided = origin_provided != 0;

	const xash::engine::server::MulticastDestinationPlan modern =
		xash::engine::server::BuildMulticastDestinationPlan(request);

	sv_multicast_destination_plan_t legacy = {};
	legacy.action = static_cast<int>(modern.action);
	legacy.visibility_mode = static_cast<int>(modern.visibilityMode);
	legacy.abort_reason = static_cast<int>(modern.abortReason);
	legacy.reliable = modern.reliable ? 1 : 0;
	legacy.single_client = modern.singleClient ? 1 : 0;
	legacy.spectator_proxy = modern.spectatorProxy ? 1 : 0;
	legacy.clear_multicast = modern.clearMulticast ? 1 : 0;
	legacy.return_value = modern.returnValue;
	return legacy;
}

extern "C" sv_multicast_recipient_decision_t SV_Multicast_BuildRecipientDecision(
	int client_state,
	int reliable,
	int user_message,
	int spectator_proxy_mode,
	int client_is_spectator_proxy,
	int has_edict,
	int fake_client,
	int prediction_filtered,
	int group_passes,
	int visible)
{
	xash::engine::server::MulticastRecipientRequest request = {};
	request.clientState = client_state;
	request.reliable = reliable != 0;
	request.userMessage = user_message != 0;
	request.spectatorProxyMode = spectator_proxy_mode != 0;
	request.clientIsSpectatorProxy = client_is_spectator_proxy != 0;
	request.hasEdict = has_edict != 0;
	request.fakeClient = fake_client != 0;
	request.predictionFiltered = prediction_filtered != 0;
	request.groupPasses = group_passes != 0;
	request.visible = visible != 0;

	const xash::engine::server::MulticastRecipientDecision modern =
		xash::engine::server::BuildMulticastRecipientDecision(request);

	sv_multicast_recipient_decision_t legacy = {};
	legacy.should_send = modern.shouldSend ? 1 : 0;
	legacy.route = static_cast<int>(modern.route);
	legacy.reason = static_cast<int>(modern.reason);
	return legacy;
}
