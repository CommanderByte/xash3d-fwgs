#include <cstdlib>

#include "engine/server/server_multicast_policy.hpp"

using namespace xash::engine::server;

namespace
{

constexpr int kClientStateFree = 0;
constexpr int kClientStateZombie = 1;
constexpr int kClientStateConnected = 2;
constexpr int kClientStateSpawned = 4;

static MulticastDestinationRequest DestinationRequest(int destination)
{
	MulticastDestinationRequest request = {};
	request.destination = destination;
	request.serverLoading = false;
	request.originProvided = true;
	return request;
}

static MulticastRecipientRequest RecipientRequest()
{
	MulticastRecipientRequest request = {};
	request.clientState = kClientStateSpawned;
	request.reliable = false;
	request.userMessage = false;
	request.spectatorProxyMode = false;
	request.clientIsSpectatorProxy = false;
	request.hasEdict = true;
	request.fakeClient = false;
	request.predictionFiltered = false;
	request.groupPasses = true;
	request.visible = true;
	return request;
}

static bool TestBroadcastIsUnreliableAllClients()
{
	const MulticastDestinationPlan plan =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationBroadcast));

	return plan.action == MulticastDestinationAction::SendToClients &&
		plan.visibilityMode == MulticastVisibilityMode::None &&
		!plan.reliable &&
		!plan.singleClient &&
		!plan.spectatorProxy &&
		plan.clearMulticast;
}

static bool TestAllAndInitOutsideLoadingAreReliable()
{
	MulticastDestinationRequest init =
		DestinationRequest(kMulticastDestinationInit);
	init.serverLoading = false;

	const MulticastDestinationPlan all =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationAll));
	const MulticastDestinationPlan initPlan =
		BuildMulticastDestinationPlan(init);

	return all.action == MulticastDestinationAction::SendToClients &&
		all.reliable &&
		initPlan.action == MulticastDestinationAction::SendToClients &&
		initPlan.reliable;
}

static bool TestInitDuringLoadingWritesSignon()
{
	MulticastDestinationRequest request =
		DestinationRequest(kMulticastDestinationInit);
	request.serverLoading = true;

	const MulticastDestinationPlan plan =
		BuildMulticastDestinationPlan(request);

	return plan.action == MulticastDestinationAction::WriteSignon &&
		plan.clearMulticast &&
		plan.returnValue == 1;
}

static bool TestPvsAndPasPlans()
{
	const MulticastDestinationPlan pvs =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationPvs));
	const MulticastDestinationPlan pvsReliable =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationPvsReliable));
	const MulticastDestinationPlan pas =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationPas));
	const MulticastDestinationPlan pasReliable =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationPasReliable));

	return pvs.visibilityMode == MulticastVisibilityMode::Pvs &&
		!pvs.reliable &&
		pvsReliable.visibilityMode == MulticastVisibilityMode::Pvs &&
		pvsReliable.reliable &&
		pas.visibilityMode == MulticastVisibilityMode::Pas &&
		!pas.reliable &&
		pasReliable.visibilityMode == MulticastVisibilityMode::Pas &&
		pasReliable.reliable;
}

static bool TestMissingOriginAbortsWithoutClearing()
{
	MulticastDestinationRequest request =
		DestinationRequest(kMulticastDestinationPvs);
	request.originProvided = false;

	const MulticastDestinationPlan plan =
		BuildMulticastDestinationPlan(request);

	return plan.action == MulticastDestinationAction::Abort &&
		plan.abortReason == MulticastAbortReason::MissingOrigin &&
		!plan.clearMulticast &&
		plan.returnValue == 0;
}

static bool TestOneClientModes()
{
	const MulticastDestinationPlan reliable =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationOne));
	const MulticastDestinationPlan unreliable =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationOneUnreliable));

	return reliable.singleClient &&
		reliable.reliable &&
		unreliable.singleClient &&
		!unreliable.reliable;
}

static bool TestSpectatorDestination()
{
	const MulticastDestinationPlan plan =
		BuildMulticastDestinationPlan(
			DestinationRequest(kMulticastDestinationSpectator));

	return plan.action == MulticastDestinationAction::SendToClients &&
		plan.reliable &&
		plan.spectatorProxy;
}

static bool TestInvalidDestinationRequestsHostError()
{
	const MulticastDestinationPlan plan =
		BuildMulticastDestinationPlan(DestinationRequest(999));

	return plan.action == MulticastDestinationAction::HostError &&
		plan.abortReason == MulticastAbortReason::InvalidDestination;
}

static bool TestSpawnedUnreliableRecipientUsesDatagram()
{
	const MulticastRecipientDecision decision =
		BuildMulticastRecipientDecision(RecipientRequest());

	return decision.shouldSend &&
		decision.route == MulticastRecipientRoute::ClientDatagram &&
		decision.reason == MulticastRecipientSkipReason::None;
}

static bool TestReliableRecipientUsesNetchan()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.reliable = true;

	const MulticastRecipientDecision decision =
		BuildMulticastRecipientDecision(request);

	return decision.shouldSend &&
		decision.route == MulticastRecipientRoute::ClientReliable;
}

static bool TestConnectedReliableNonUserMessageCanReceive()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.clientState = kClientStateConnected;
	request.reliable = true;
	request.userMessage = false;

	const MulticastRecipientDecision decision =
		BuildMulticastRecipientDecision(request);

	return decision.shouldSend &&
		decision.route == MulticastRecipientRoute::ClientReliable;
}

static bool TestConnectedUnreliableNeedsSpawned()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.clientState = kClientStateConnected;
	request.reliable = false;

	const MulticastRecipientDecision decision =
		BuildMulticastRecipientDecision(request);

	return !decision.shouldSend &&
		decision.reason == MulticastRecipientSkipReason::NeedsSpawnedClient;
}

static bool TestConnectedUserMessageNeedsSpawnedEvenWhenReliable()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.clientState = kClientStateConnected;
	request.reliable = true;
	request.userMessage = true;

	const MulticastRecipientDecision decision =
		BuildMulticastRecipientDecision(request);

	return !decision.shouldSend &&
		decision.reason == MulticastRecipientSkipReason::NeedsSpawnedClient;
}

static bool TestSpectatorProxyMode()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.spectatorProxyMode = true;
	request.clientIsSpectatorProxy = true;

	const MulticastRecipientDecision allow =
		BuildMulticastRecipientDecision(request);

	request.clientIsSpectatorProxy = false;
	const MulticastRecipientDecision reject =
		BuildMulticastRecipientDecision(request);

	return allow.shouldSend &&
		allow.route == MulticastRecipientRoute::SpectatorDatagram &&
		!reject.shouldSend &&
		reject.reason == MulticastRecipientSkipReason::NotSpectatorProxy;
}

static bool TestRecipientRejectsLegacySkips()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.clientState = kClientStateFree;

	const MulticastRecipientDecision freeClient =
		BuildMulticastRecipientDecision(request);

	request = RecipientRequest();
	request.clientState = kClientStateZombie;
	const MulticastRecipientDecision zombie =
		BuildMulticastRecipientDecision(request);

	request = RecipientRequest();
	request.hasEdict = false;
	const MulticastRecipientDecision missingEdict =
		BuildMulticastRecipientDecision(request);

	request = RecipientRequest();
	request.fakeClient = true;
	const MulticastRecipientDecision fake =
		BuildMulticastRecipientDecision(request);

	return !freeClient.shouldSend &&
		freeClient.reason == MulticastRecipientSkipReason::FreeOrZombie &&
		!zombie.shouldSend &&
		zombie.reason == MulticastRecipientSkipReason::FreeOrZombie &&
		!missingEdict.shouldSend &&
		missingEdict.reason == MulticastRecipientSkipReason::MissingEdict &&
		!fake.shouldSend &&
		fake.reason == MulticastRecipientSkipReason::FakeClient;
}

static bool TestRecipientRejectsFilters()
{
	MulticastRecipientRequest request = RecipientRequest();
	request.predictionFiltered = true;
	const MulticastRecipientDecision prediction =
		BuildMulticastRecipientDecision(request);

	request = RecipientRequest();
	request.groupPasses = false;
	const MulticastRecipientDecision group =
		BuildMulticastRecipientDecision(request);

	request = RecipientRequest();
	request.visible = false;
	const MulticastRecipientDecision visibility =
		BuildMulticastRecipientDecision(request);

	return !prediction.shouldSend &&
		prediction.reason == MulticastRecipientSkipReason::PredictionFiltered &&
		!group.shouldSend &&
		group.reason == MulticastRecipientSkipReason::GroupFiltered &&
		!visibility.shouldSend &&
		visibility.reason == MulticastRecipientSkipReason::NotVisible;
}

}

int main()
{
	if (!TestBroadcastIsUnreliableAllClients() ||
		!TestAllAndInitOutsideLoadingAreReliable() ||
		!TestInitDuringLoadingWritesSignon() ||
		!TestPvsAndPasPlans() ||
		!TestMissingOriginAbortsWithoutClearing() ||
		!TestOneClientModes() ||
		!TestSpectatorDestination() ||
		!TestInvalidDestinationRequestsHostError() ||
		!TestSpawnedUnreliableRecipientUsesDatagram() ||
		!TestReliableRecipientUsesNetchan() ||
		!TestConnectedReliableNonUserMessageCanReceive() ||
		!TestConnectedUnreliableNeedsSpawned() ||
		!TestConnectedUserMessageNeedsSpawnedEvenWhenReliable() ||
		!TestSpectatorProxyMode() ||
		!TestRecipientRejectsLegacySkips() ||
		!TestRecipientRejectsFilters())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
