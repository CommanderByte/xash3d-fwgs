#include "engine/server/messaging/server_event_playback_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{

bool ServerEventPlaybackAllowsServer(unsigned int flags)
{
	return (flags & kServerEventFlagClient) == 0u;
}

bool ServerEventPlaybackUsesReliableDelivery(unsigned int flags)
{
	return (flags & kServerEventFlagReliable) != 0u;
}

ServerEventPlaybackFlagPlan BuildServerEventPlaybackFlagPlan(
	unsigned int flags,
	float delay,
	bool clientInvoker)
{
	ServerEventPlaybackFlagPlan plan = {};
	plan.allowPlayback = ServerEventPlaybackAllowsServer(flags);
	plan.flags = flags;
	plan.delay = delay < 0.0f ? 0.0f : delay;

	if (!clientInvoker)
	{
		if ((plan.flags & kServerEventFlagNotHost) != 0u)
		{
			plan.flags &= ~kServerEventFlagNotHost;
			plan.clearedNotHost = true;
		}

		if ((plan.flags & kServerEventFlagHostOnly) != 0u)
		{
			plan.flags &= ~kServerEventFlagHostOnly;
			plan.clearedHostOnly = true;
		}
	}

	plan.flags |= kServerEventFlagServer;
	plan.reliable = ServerEventPlaybackUsesReliableDelivery(plan.flags);
	return plan;
}

ServerEventRecipientDecision BuildServerEventRecipientDecision(
	const ServerEventRecipientInput &input)
{
	ServerEventRecipientDecision decision = {};
	decision.deliver = false;

	if (!input.spawned)
	{
		decision.rejectReason = ServerEventRecipientRejectReason::NotSpawned;
		return decision;
	}

	if (!input.hasEdict)
	{
		decision.rejectReason = ServerEventRecipientRejectReason::MissingEdict;
		return decision;
	}

	if (input.fakeClient)
	{
		decision.rejectReason = ServerEventRecipientRejectReason::FakeClient;
		return decision;
	}

	if (input.invokerValid && !input.groupPasses)
	{
		decision.rejectReason = ServerEventRecipientRejectReason::GroupFilter;
		return decision;
	}

	if (input.invokerValid && !input.visible)
	{
		decision.rejectReason = ServerEventRecipientRejectReason::NotVisible;
		return decision;
	}

	if (input.notHost &&
		input.localWeapons &&
		(input.currentClient || input.invokerClient))
	{
		decision.rejectReason =
			ServerEventRecipientRejectReason::NotHostLocalWeapons;
		return decision;
	}

	if (input.hostOnly && !input.invokerClient)
	{
		decision.rejectReason =
			ServerEventRecipientRejectReason::HostOnlyDifferentClient;
		return decision;
	}

	decision.deliver = true;
	decision.rejectReason = ServerEventRecipientRejectReason::None;
	return decision;
}

ServerEventQueueSlotPlan SelectServerEventQueueSlot(
	const ServerEventQueueSlotSnapshot *slots,
	int slotCount,
	unsigned int eventIndex,
	int invokerIndex,
	bool updateEvent)
{
	ServerEventQueueSlotPlan plan = {};
	plan.slot = -1;

	if (!slots || slotCount <= 0)
		return plan;

	if (updateEvent)
	{
		for (int i = 0; i < slotCount; ++i)
		{
			if (slots[i].eventIndex == eventIndex &&
				invokerIndex != -1 &&
				slots[i].entityIndex == invokerIndex)
			{
				plan.hasSlot = true;
				plan.slot = i;
				plan.reusedUpdateSlot = true;
				return plan;
			}
		}
	}

	for (int i = 0; i < slotCount; ++i)
	{
		if (slots[i].eventIndex == 0u)
		{
			plan.hasSlot = true;
			plan.slot = i;
			return plan;
		}
	}

	return plan;
}

int ClampServerEventEmitCount(int queuedCount, int maxQueue)
{
	if (queuedCount <= 0 || maxQueue <= 0)
		return 0;

	const int limit = maxQueue / 2;
	if (limit <= 0)
		return 0;

	if (queuedCount >= limit)
		return limit - 1;

	return queuedCount;
}

}
}
}
