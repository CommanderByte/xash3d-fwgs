#ifndef XASH_ENGINE_SERVER_EVENT_PLAYBACK_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_EVENT_PLAYBACK_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_event_playback_flag_plan_s
{
	int allow_playback;
	unsigned int flags;
	float delay;
	int cleared_not_host;
	int cleared_host_only;
	int reliable;
} sv_event_playback_flag_plan_t;

typedef struct sv_event_recipient_input_s
{
	int spawned;
	int has_edict;
	int fake_client;
	int invoker_valid;
	int group_passes;
	int visible;
	int not_host;
	int host_only;
	int local_weapons;
	int current_client;
	int invoker_client;
} sv_event_recipient_input_t;

typedef struct sv_event_recipient_decision_s
{
	int deliver;
	int reject_reason;
} sv_event_recipient_decision_t;

typedef struct sv_event_queue_slot_s
{
	unsigned int event_index;
	int entity_index;
} sv_event_queue_slot_t;

typedef struct sv_event_queue_slot_plan_s
{
	int has_slot;
	int slot;
	int reused_update_slot;
} sv_event_queue_slot_plan_t;

int SV_EventPlayback_AllowsServer(unsigned int flags);
int SV_EventPlayback_UsesReliableDelivery(unsigned int flags);

sv_event_playback_flag_plan_t SV_EventPlayback_BuildFlagPlan(
	unsigned int flags,
	float delay,
	int client_invoker);

sv_event_recipient_decision_t SV_EventPlayback_BuildRecipientDecision(
	const sv_event_recipient_input_t *input);

sv_event_queue_slot_plan_t SV_EventPlayback_SelectQueueSlot(
	const sv_event_queue_slot_t *slots,
	int slot_count,
	unsigned int event_index,
	int invoker_index,
	int update_event);

int SV_EventPlayback_ClampEmitCount(int queued_count, int max_queue);

#ifdef __cplusplus
}
#endif

#endif
