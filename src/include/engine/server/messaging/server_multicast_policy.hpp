#ifndef XASH_ENGINE_SERVER_SERVER_MULTICAST_POLICY_HPP
#define XASH_ENGINE_SERVER_SERVER_MULTICAST_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kMulticastDestinationBroadcast = 0;
constexpr int kMulticastDestinationOne = 1;
constexpr int kMulticastDestinationAll = 2;
constexpr int kMulticastDestinationInit = 3;
constexpr int kMulticastDestinationPvs = 4;
constexpr int kMulticastDestinationPas = 5;
constexpr int kMulticastDestinationPvsReliable = 6;
constexpr int kMulticastDestinationPasReliable = 7;
constexpr int kMulticastDestinationOneUnreliable = 8;
constexpr int kMulticastDestinationSpectator = 9;

enum class MulticastDestinationAction
{
	SendToClients,
	WriteSignon,
	Abort,
	HostError
};

enum class MulticastVisibilityMode
{
	None,
	Pvs,
	Pas
};

enum class MulticastAbortReason
{
	None,
	MissingOrigin,
	InvalidDestination
};

struct MulticastDestinationRequest
{
	int destination;
	bool serverLoading;
	bool originProvided;
};

struct MulticastDestinationPlan
{
	MulticastDestinationAction action;
	MulticastVisibilityMode visibilityMode;
	MulticastAbortReason abortReason;
	bool reliable;
	bool singleClient;
	bool spectatorProxy;
	bool clearMulticast;
	int returnValue;
};

enum class MulticastRecipientRoute
{
	None,
	ClientDatagram,
	ClientReliable,
	SpectatorDatagram
};

enum class MulticastRecipientSkipReason
{
	None,
	FreeOrZombie,
	NeedsSpawnedClient,
	NotSpectatorProxy,
	MissingEdict,
	FakeClient,
	PredictionFiltered,
	GroupFiltered,
	NotVisible
};

struct MulticastRecipientRequest
{
	int clientState;
	bool reliable;
	bool userMessage;
	bool spectatorProxyMode;
	bool clientIsSpectatorProxy;
	bool hasEdict;
	bool fakeClient;
	bool predictionFiltered;
	bool groupPasses;
	bool visible;
};

struct MulticastRecipientDecision
{
	bool shouldSend;
	MulticastRecipientRoute route;
	MulticastRecipientSkipReason reason;
};

MulticastDestinationPlan BuildMulticastDestinationPlan(
	const MulticastDestinationRequest &request);
MulticastRecipientDecision BuildMulticastRecipientDecision(
	const MulticastRecipientRequest &request);

}
}
}

#endif
