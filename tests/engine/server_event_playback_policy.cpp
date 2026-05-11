#include <cstdlib>

#include "engine/server/messaging/server_event_playback_policy.hpp"

using namespace xash::engine::server;

namespace
{

ServerEventRecipientInput DefaultRecipient()
{
	ServerEventRecipientInput input = {};
	input.spawned = true;
	input.hasEdict = true;
	input.groupPasses = true;
	input.visible = true;
	return input;
}

bool TestServerAdmissionAndReliableFlag()
{
	return ServerEventPlaybackAllowsServer(0u) &&
		!ServerEventPlaybackAllowsServer(kServerEventFlagClient) &&
		ServerEventPlaybackUsesReliableDelivery(kServerEventFlagReliable) &&
		!ServerEventPlaybackUsesReliableDelivery(kServerEventFlagNotHost);
}

bool TestFlagNormalizationForNonClientInvoker()
{
	const ServerEventPlaybackFlagPlan plan =
		BuildServerEventPlaybackFlagPlan(
			kServerEventFlagNotHost |
				kServerEventFlagHostOnly |
				kServerEventFlagReliable,
			-2.5f,
			false);

	return plan.allowPlayback &&
		(plan.flags & kServerEventFlagServer) != 0u &&
		(plan.flags & kServerEventFlagReliable) != 0u &&
		(plan.flags & kServerEventFlagNotHost) == 0u &&
		(plan.flags & kServerEventFlagHostOnly) == 0u &&
		plan.delay == 0.0f &&
		plan.clearedNotHost &&
		plan.clearedHostOnly &&
		plan.reliable;
}

bool TestFlagNormalizationForClientInvoker()
{
	const ServerEventPlaybackFlagPlan plan =
		BuildServerEventPlaybackFlagPlan(
			kServerEventFlagNotHost | kServerEventFlagHostOnly,
			0.25f,
			true);

	return plan.allowPlayback &&
		(plan.flags & kServerEventFlagServer) != 0u &&
		(plan.flags & kServerEventFlagNotHost) != 0u &&
		(plan.flags & kServerEventFlagHostOnly) != 0u &&
		plan.delay == 0.25f &&
		!plan.clearedNotHost &&
		!plan.clearedHostOnly &&
		!plan.reliable;
}

bool TestRecipientBasicRejections()
{
	ServerEventRecipientInput input = DefaultRecipient();
	input.spawned = false;
	const ServerEventRecipientDecision notSpawned =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.hasEdict = false;
	const ServerEventRecipientDecision missingEdict =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.fakeClient = true;
	const ServerEventRecipientDecision fakeClient =
		BuildServerEventRecipientDecision(input);

	return !notSpawned.deliver &&
		notSpawned.rejectReason ==
			ServerEventRecipientRejectReason::NotSpawned &&
		!missingEdict.deliver &&
		missingEdict.rejectReason ==
			ServerEventRecipientRejectReason::MissingEdict &&
		!fakeClient.deliver &&
		fakeClient.rejectReason ==
			ServerEventRecipientRejectReason::FakeClient;
}

bool TestRecipientPolicyDoesNotHaveSpectatorSpecificGate()
{
	ServerEventRecipientInput input = DefaultRecipient();

	return BuildServerEventRecipientDecision(input).deliver;
}

bool TestRecipientInvokerVisibilityRejections()
{
	ServerEventRecipientInput input = DefaultRecipient();
	input.invokerValid = true;
	input.groupPasses = false;
	const ServerEventRecipientDecision groupRejected =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.invokerValid = true;
	input.visible = false;
	const ServerEventRecipientDecision invisible =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.invokerValid = false;
	input.groupPasses = false;
	input.visible = false;
	const ServerEventRecipientDecision noInvoker =
		BuildServerEventRecipientDecision(input);

	return !groupRejected.deliver &&
		groupRejected.rejectReason ==
			ServerEventRecipientRejectReason::GroupFilter &&
		!invisible.deliver &&
		invisible.rejectReason ==
			ServerEventRecipientRejectReason::NotVisible &&
		noInvoker.deliver &&
		noInvoker.rejectReason == ServerEventRecipientRejectReason::None;
}

bool TestRecipientNotHostLocalWeaponsRule()
{
	ServerEventRecipientInput input = DefaultRecipient();
	input.notHost = true;
	input.localWeapons = true;
	input.currentClient = true;
	const ServerEventRecipientDecision currentClient =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.notHost = true;
	input.localWeapons = true;
	input.invokerClient = true;
	const ServerEventRecipientDecision invokerClient =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.notHost = true;
	input.localWeapons = false;
	input.currentClient = true;
	input.invokerClient = true;
	const ServerEventRecipientDecision noLocalWeapons =
		BuildServerEventRecipientDecision(input);

	return !currentClient.deliver &&
		currentClient.rejectReason ==
			ServerEventRecipientRejectReason::NotHostLocalWeapons &&
		!invokerClient.deliver &&
		invokerClient.rejectReason ==
			ServerEventRecipientRejectReason::NotHostLocalWeapons &&
		noLocalWeapons.deliver;
}

bool TestRecipientHostOnlyRule()
{
	ServerEventRecipientInput input = DefaultRecipient();
	input.hostOnly = true;
	input.invokerClient = false;
	const ServerEventRecipientDecision otherClient =
		BuildServerEventRecipientDecision(input);

	input = DefaultRecipient();
	input.hostOnly = true;
	input.invokerClient = true;
	const ServerEventRecipientDecision invokerClient =
		BuildServerEventRecipientDecision(input);

	return !otherClient.deliver &&
		otherClient.rejectReason ==
			ServerEventRecipientRejectReason::HostOnlyDifferentClient &&
		invokerClient.deliver;
}

bool TestQueueSlotSelectionReusesUpdateSlot()
{
	const ServerEventQueueSlotSnapshot slots[] = {
		{5u, 1},
		{9u, 2},
		{0u, -1},
	};

	const ServerEventQueueSlotPlan plan =
		SelectServerEventQueueSlot(slots, 3, 9u, 2, true);

	return plan.hasSlot &&
		plan.slot == 1 &&
		plan.reusedUpdateSlot;
}

bool TestQueueSlotSelectionFallsBackToEmptySlot()
{
	const ServerEventQueueSlotSnapshot slots[] = {
		{5u, 1},
		{9u, 2},
		{0u, -1},
	};

	const ServerEventQueueSlotPlan noInvoker =
		SelectServerEventQueueSlot(slots, 3, 9u, -1, true);
	const ServerEventQueueSlotPlan nonUpdate =
		SelectServerEventQueueSlot(slots, 3, 9u, 2, false);

	return noInvoker.hasSlot &&
		noInvoker.slot == 2 &&
		!noInvoker.reusedUpdateSlot &&
		nonUpdate.hasSlot &&
		nonUpdate.slot == 2 &&
		!nonUpdate.reusedUpdateSlot;
}

bool TestQueueSlotSelectionReportsFullQueue()
{
	const ServerEventQueueSlotSnapshot slots[] = {
		{5u, 1},
		{9u, 2},
	};

	const ServerEventQueueSlotPlan plan =
		SelectServerEventQueueSlot(slots, 2, 7u, 3, true);

	return !plan.hasSlot &&
		plan.slot == -1 &&
		!plan.reusedUpdateSlot;
}

bool TestEmitCountClamp()
{
	return ClampServerEventEmitCount(0, 64) == 0 &&
		ClampServerEventEmitCount(4, 64) == 4 &&
		ClampServerEventEmitCount(32, 64) == 31 &&
		ClampServerEventEmitCount(64, 64) == 31;
}

}

int main()
{
	if (!TestServerAdmissionAndReliableFlag() ||
		!TestFlagNormalizationForNonClientInvoker() ||
		!TestFlagNormalizationForClientInvoker() ||
		!TestRecipientBasicRejections() ||
		!TestRecipientPolicyDoesNotHaveSpectatorSpecificGate() ||
		!TestRecipientInvokerVisibilityRejections() ||
		!TestRecipientNotHostLocalWeaponsRule() ||
		!TestRecipientHostOnlyRule() ||
		!TestQueueSlotSelectionReusesUpdateSlot() ||
		!TestQueueSlotSelectionFallsBackToEmptySlot() ||
		!TestQueueSlotSelectionReportsFullQueue() ||
		!TestEmitCountClamp())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
