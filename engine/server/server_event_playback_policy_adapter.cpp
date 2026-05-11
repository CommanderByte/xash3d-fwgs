#include "server_event_playback_policy_adapter.h"

#include "engine/server/server_event_playback_policy.hpp"

namespace
{

constexpr int kAdapterMaxEventQueueSlots = 64;

sv_event_playback_flag_plan_t ToLegacyFlagPlan(
	const xash::engine::server::ServerEventPlaybackFlagPlan &plan)
{
	sv_event_playback_flag_plan_t legacy = {};
	legacy.allow_playback = plan.allowPlayback ? 1 : 0;
	legacy.flags = plan.flags;
	legacy.delay = plan.delay;
	legacy.cleared_not_host = plan.clearedNotHost ? 1 : 0;
	legacy.cleared_host_only = plan.clearedHostOnly ? 1 : 0;
	legacy.reliable = plan.reliable ? 1 : 0;
	return legacy;
}

xash::engine::server::ServerEventRecipientInput ToModernRecipientInput(
	const sv_event_recipient_input_t &input)
{
	xash::engine::server::ServerEventRecipientInput modern = {};
	modern.spawned = input.spawned != 0;
	modern.hasEdict = input.has_edict != 0;
	modern.fakeClient = input.fake_client != 0;
	modern.invokerValid = input.invoker_valid != 0;
	modern.groupPasses = input.group_passes != 0;
	modern.visible = input.visible != 0;
	modern.notHost = input.not_host != 0;
	modern.hostOnly = input.host_only != 0;
	modern.localWeapons = input.local_weapons != 0;
	modern.currentClient = input.current_client != 0;
	modern.invokerClient = input.invoker_client != 0;
	return modern;
}

sv_event_recipient_decision_t ToLegacyRecipientDecision(
	const xash::engine::server::ServerEventRecipientDecision &decision)
{
	sv_event_recipient_decision_t legacy = {};
	legacy.deliver = decision.deliver ? 1 : 0;
	legacy.reject_reason = static_cast<int>(decision.rejectReason);
	return legacy;
}

}

extern "C" int SV_EventPlayback_AllowsServer(unsigned int flags)
{
	return xash::engine::server::ServerEventPlaybackAllowsServer(flags)
		? 1
		: 0;
}

extern "C" int SV_EventPlayback_UsesReliableDelivery(unsigned int flags)
{
	return xash::engine::server::ServerEventPlaybackUsesReliableDelivery(flags)
		? 1
		: 0;
}

extern "C" sv_event_playback_flag_plan_t SV_EventPlayback_BuildFlagPlan(
	unsigned int flags,
	float delay,
	int client_invoker)
{
	return ToLegacyFlagPlan(
		xash::engine::server::BuildServerEventPlaybackFlagPlan(
			flags,
			delay,
			client_invoker != 0));
}

extern "C" sv_event_recipient_decision_t
SV_EventPlayback_BuildRecipientDecision(
	const sv_event_recipient_input_t *input)
{
	if (!input)
		return {};

	return ToLegacyRecipientDecision(
		xash::engine::server::BuildServerEventRecipientDecision(
			ToModernRecipientInput(*input)));
}

extern "C" sv_event_queue_slot_plan_t SV_EventPlayback_SelectQueueSlot(
	const sv_event_queue_slot_t *slots,
	int slot_count,
	unsigned int event_index,
	int invoker_index,
	int update_event)
{
	sv_event_queue_slot_plan_t legacy = {};
	legacy.slot = -1;

	if (!slots || slot_count <= 0)
		return legacy;

	if (slot_count > kAdapterMaxEventQueueSlots)
		slot_count = kAdapterMaxEventQueueSlots;

	xash::engine::server::ServerEventQueueSlotSnapshot
		modernSlots[kAdapterMaxEventQueueSlots];

	for (int i = 0; i < slot_count; ++i)
	{
		modernSlots[i].eventIndex = slots[i].event_index;
		modernSlots[i].entityIndex = slots[i].entity_index;
	}

	const xash::engine::server::ServerEventQueueSlotPlan modern =
		xash::engine::server::SelectServerEventQueueSlot(
			modernSlots,
			slot_count,
			event_index,
			invoker_index,
			update_event != 0);

	legacy.has_slot = modern.hasSlot ? 1 : 0;
	legacy.slot = modern.slot;
	legacy.reused_update_slot = modern.reusedUpdateSlot ? 1 : 0;
	return legacy;
}

extern "C" int SV_EventPlayback_ClampEmitCount(
	int queued_count,
	int max_queue)
{
	return xash::engine::server::ClampServerEventEmitCount(
		queued_count,
		max_queue);
}
