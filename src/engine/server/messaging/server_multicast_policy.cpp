#include "engine/server/messaging/server_multicast_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr int kClientStateFree = 0;
constexpr int kClientStateZombie = 1;
constexpr int kClientStateSpawned = 4;

MulticastDestinationPlan MakeSendPlan(
	bool reliable,
	bool singleClient,
	bool spectatorProxy,
	MulticastVisibilityMode visibilityMode)
{
	MulticastDestinationPlan plan = {};
	plan.action = MulticastDestinationAction::SendToClients;
	plan.visibilityMode = visibilityMode;
	plan.reliable = reliable;
	plan.singleClient = singleClient;
	plan.spectatorProxy = spectatorProxy;
	plan.clearMulticast = true;
	return plan;
}

MulticastDestinationPlan MakeAbortPlan(MulticastAbortReason reason)
{
	MulticastDestinationPlan plan = {};
	plan.action = MulticastDestinationAction::Abort;
	plan.abortReason = reason;
	plan.clearMulticast = false;
	return plan;
}

MulticastRecipientDecision MakeSkip(MulticastRecipientSkipReason reason)
{
	MulticastRecipientDecision decision = {};
	decision.reason = reason;
	return decision;
}

}

MulticastDestinationPlan BuildMulticastDestinationPlan(
	const MulticastDestinationRequest &request)
{
	switch (request.destination)
	{
	case kMulticastDestinationInit:
		if (request.serverLoading)
		{
			MulticastDestinationPlan plan = {};
			plan.action = MulticastDestinationAction::WriteSignon;
			plan.clearMulticast = true;
			plan.returnValue = 1;
			return plan;
		}
		return MakeSendPlan(true, false, false, MulticastVisibilityMode::None);
	case kMulticastDestinationAll:
		return MakeSendPlan(true, false, false, MulticastVisibilityMode::None);
	case kMulticastDestinationBroadcast:
		return MakeSendPlan(false, false, false, MulticastVisibilityMode::None);
	case kMulticastDestinationPasReliable:
		if (!request.originProvided)
			return MakeAbortPlan(MulticastAbortReason::MissingOrigin);
		return MakeSendPlan(true, false, false, MulticastVisibilityMode::Pas);
	case kMulticastDestinationPas:
		if (!request.originProvided)
			return MakeAbortPlan(MulticastAbortReason::MissingOrigin);
		return MakeSendPlan(false, false, false, MulticastVisibilityMode::Pas);
	case kMulticastDestinationPvsReliable:
		if (!request.originProvided)
			return MakeAbortPlan(MulticastAbortReason::MissingOrigin);
		return MakeSendPlan(true, false, false, MulticastVisibilityMode::Pvs);
	case kMulticastDestinationPvs:
		if (!request.originProvided)
			return MakeAbortPlan(MulticastAbortReason::MissingOrigin);
		return MakeSendPlan(false, false, false, MulticastVisibilityMode::Pvs);
	case kMulticastDestinationOne:
		return MakeSendPlan(true, true, false, MulticastVisibilityMode::None);
	case kMulticastDestinationOneUnreliable:
		return MakeSendPlan(false, true, false, MulticastVisibilityMode::None);
	case kMulticastDestinationSpectator:
		return MakeSendPlan(true, false, true, MulticastVisibilityMode::None);
	default:
	{
		MulticastDestinationPlan plan = {};
		plan.action = MulticastDestinationAction::HostError;
		plan.abortReason = MulticastAbortReason::InvalidDestination;
		return plan;
	}
	}
}

MulticastRecipientDecision BuildMulticastRecipientDecision(
	const MulticastRecipientRequest &request)
{
	if (request.clientState == kClientStateFree ||
		request.clientState == kClientStateZombie)
	{
		return MakeSkip(MulticastRecipientSkipReason::FreeOrZombie);
	}

	if (request.clientState != kClientStateSpawned &&
		(!request.reliable || request.userMessage))
	{
		return MakeSkip(MulticastRecipientSkipReason::NeedsSpawnedClient);
	}

	if (request.spectatorProxyMode && !request.clientIsSpectatorProxy)
		return MakeSkip(MulticastRecipientSkipReason::NotSpectatorProxy);

	if (!request.hasEdict)
		return MakeSkip(MulticastRecipientSkipReason::MissingEdict);

	if (request.fakeClient)
		return MakeSkip(MulticastRecipientSkipReason::FakeClient);

	if (request.predictionFiltered)
		return MakeSkip(MulticastRecipientSkipReason::PredictionFiltered);

	if (!request.groupPasses)
		return MakeSkip(MulticastRecipientSkipReason::GroupFiltered);

	if (!request.visible)
		return MakeSkip(MulticastRecipientSkipReason::NotVisible);

	MulticastRecipientDecision decision = {};
	decision.shouldSend = true;
	decision.route = request.spectatorProxyMode
		? MulticastRecipientRoute::SpectatorDatagram
		: (request.reliable
			? MulticastRecipientRoute::ClientReliable
			: MulticastRecipientRoute::ClientDatagram);
	return decision;
}

}
}
}
