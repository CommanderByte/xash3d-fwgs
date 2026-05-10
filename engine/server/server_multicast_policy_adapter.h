#ifndef XASH_ENGINE_SERVER_MULTICAST_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_MULTICAST_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	SV_MULTICAST_ACTION_SEND_TO_CLIENTS = 0,
	SV_MULTICAST_ACTION_WRITE_SIGNON = 1,
	SV_MULTICAST_ACTION_ABORT = 2,
	SV_MULTICAST_ACTION_HOST_ERROR = 3
};

enum
{
	SV_MULTICAST_VISIBILITY_NONE = 0,
	SV_MULTICAST_VISIBILITY_PVS = 1,
	SV_MULTICAST_VISIBILITY_PAS = 2
};

enum
{
	SV_MULTICAST_ABORT_NONE = 0,
	SV_MULTICAST_ABORT_MISSING_ORIGIN = 1,
	SV_MULTICAST_ABORT_INVALID_DESTINATION = 2
};

enum
{
	SV_MULTICAST_ROUTE_NONE = 0,
	SV_MULTICAST_ROUTE_CLIENT_DATAGRAM = 1,
	SV_MULTICAST_ROUTE_CLIENT_RELIABLE = 2,
	SV_MULTICAST_ROUTE_SPECTATOR_DATAGRAM = 3
};

enum
{
	SV_MULTICAST_SKIP_NONE = 0,
	SV_MULTICAST_SKIP_FREE_OR_ZOMBIE = 1,
	SV_MULTICAST_SKIP_NEEDS_SPAWNED_CLIENT = 2,
	SV_MULTICAST_SKIP_NOT_SPECTATOR_PROXY = 3,
	SV_MULTICAST_SKIP_MISSING_EDICT = 4,
	SV_MULTICAST_SKIP_FAKE_CLIENT = 5,
	SV_MULTICAST_SKIP_PREDICTION_FILTERED = 6,
	SV_MULTICAST_SKIP_GROUP_FILTERED = 7,
	SV_MULTICAST_SKIP_NOT_VISIBLE = 8
};

typedef struct sv_multicast_destination_plan_s
{
	int action;
	int visibility_mode;
	int abort_reason;
	int reliable;
	int single_client;
	int spectator_proxy;
	int clear_multicast;
	int return_value;
} sv_multicast_destination_plan_t;

typedef struct sv_multicast_recipient_decision_s
{
	int should_send;
	int route;
	int reason;
} sv_multicast_recipient_decision_t;

sv_multicast_destination_plan_t SV_Multicast_BuildDestinationPlan(
	int destination,
	int server_loading,
	int origin_provided);

sv_multicast_recipient_decision_t SV_Multicast_BuildRecipientDecision(
	int client_state,
	int reliable,
	int user_message,
	int spectator_proxy_mode,
	int client_is_spectator_proxy,
	int has_edict,
	int fake_client,
	int prediction_filtered,
	int group_passes,
	int visible);

#ifdef __cplusplus
}
#endif

#endif
