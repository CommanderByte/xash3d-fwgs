#ifndef XASH_ENGINE_SERVER_SERVER_EVENT_PLAYBACK_POLICY_HPP
#define XASH_ENGINE_SERVER_SERVER_EVENT_PLAYBACK_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr unsigned int kServerEventFlagNotHost = 1u << 0;
constexpr unsigned int kServerEventFlagReliable = 1u << 1;
constexpr unsigned int kServerEventFlagGlobal = 1u << 2;
constexpr unsigned int kServerEventFlagUpdate = 1u << 3;
constexpr unsigned int kServerEventFlagHostOnly = 1u << 4;
constexpr unsigned int kServerEventFlagServer = 1u << 5;
constexpr unsigned int kServerEventFlagClient = 1u << 6;

enum class ServerEventRecipientRejectReason
{
	None,
	NotSpawned,
	MissingEdict,
	FakeClient,
	GroupFilter,
	NotVisible,
	NotHostLocalWeapons,
	HostOnlyDifferentClient,
};

struct ServerEventPlaybackFlagPlan
{
	bool allowPlayback;
	unsigned int flags;
	float delay;
	bool clearedNotHost;
	bool clearedHostOnly;
	bool reliable;
};

struct ServerEventRecipientInput
{
	bool spawned;
	bool hasEdict;
	bool fakeClient;
	bool invokerValid;
	bool groupPasses;
	bool visible;
	bool notHost;
	bool hostOnly;
	bool localWeapons;
	bool currentClient;
	bool invokerClient;
};

struct ServerEventRecipientDecision
{
	bool deliver;
	ServerEventRecipientRejectReason rejectReason;
};

struct ServerEventQueueSlotSnapshot
{
	unsigned int eventIndex;
	int entityIndex;
};

struct ServerEventQueueSlotPlan
{
	bool hasSlot;
	int slot;
	bool reusedUpdateSlot;
};

bool ServerEventPlaybackAllowsServer(unsigned int flags);
bool ServerEventPlaybackUsesReliableDelivery(unsigned int flags);

ServerEventPlaybackFlagPlan BuildServerEventPlaybackFlagPlan(
	unsigned int flags,
	float delay,
	bool clientInvoker);

ServerEventRecipientDecision BuildServerEventRecipientDecision(
	const ServerEventRecipientInput &input);

ServerEventQueueSlotPlan SelectServerEventQueueSlot(
	const ServerEventQueueSlotSnapshot *slots,
	int slotCount,
	unsigned int eventIndex,
	int invokerIndex,
	bool updateEvent);

int ClampServerEventEmitCount(int queuedCount, int maxQueue);

}
}
}

#endif
