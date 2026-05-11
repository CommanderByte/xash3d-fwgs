#ifndef XASH_ENGINE_SERVER_CLIENT_SESSION_SLOTS_HPP
#define XASH_ENGINE_SERVER_CLIENT_SESSION_SLOTS_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kClientSessionSlotFree = 0;
constexpr int kClientSessionSlotZombie = 1;
constexpr int kClientSessionSlotConnected = 2;
constexpr int kClientSessionSlotSpawning = 3;
constexpr int kClientSessionSlotSpawned = 4;

struct ClientSessionSlotSnapshot
{
	int state;
	unsigned int flags;
};

struct ClientSessionPopulation
{
	int players;
	int bots;
	int connected;
};

enum class ClientSessionMasterUpdate
{
	None = 0,
	FirstConnectedClient = 1,
	FullServer = 2,
	EmptyServer = 3,
};

bool ClientSessionSlotIsFree(int state);
bool ClientSessionSlotIsConnected(int state);
bool ClientSessionSlotIsFakeClient(unsigned int flags);

ClientSessionPopulation CountClientSessionPopulation(
	const ClientSessionSlotSnapshot *slots,
	int slotCount);

int FindFirstFreeClientSessionSlot(
	const ClientSessionSlotSnapshot *slots,
	int slotCount);

ClientSessionMasterUpdate BuildClientSessionConnectMasterUpdate(
	int connectedSlots,
	int maxSlots);

ClientSessionMasterUpdate BuildClientSessionDropMasterUpdate(
	int connectedSlots);

}
}
}

#endif
