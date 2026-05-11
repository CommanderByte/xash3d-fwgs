#include "engine/server/client_session_slots.hpp"

#include "engine/server/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{

bool ClientSessionSlotIsFree(int state)
{
	return state == kClientSessionSlotFree;
}

bool ClientSessionSlotIsConnected(int state)
{
	return state >= kClientSessionSlotConnected;
}

bool ClientSessionSlotIsFakeClient(unsigned int flags)
{
	return (flags & kServerClientFlagFakeClient) != 0u;
}

ClientSessionPopulation CountClientSessionPopulation(
	const ClientSessionSlotSnapshot *slots,
	int slotCount)
{
	ClientSessionPopulation population = {};

	if (!slots || slotCount <= 0)
		return population;

	for (int i = 0; i < slotCount; ++i)
	{
		if (!ClientSessionSlotIsConnected(slots[i].state))
			continue;

		++population.connected;

		if (ClientSessionSlotIsFakeClient(slots[i].flags))
			++population.bots;
		else
			++population.players;
	}

	return population;
}

int FindFirstFreeClientSessionSlot(
	const ClientSessionSlotSnapshot *slots,
	int slotCount)
{
	if (!slots || slotCount <= 0)
		return -1;

	for (int i = 0; i < slotCount; ++i)
	{
		if (ClientSessionSlotIsFree(slots[i].state))
			return i;
	}

	return -1;
}

ClientSessionMasterUpdate BuildClientSessionConnectMasterUpdate(
	int connectedSlots,
	int maxSlots)
{
	if (connectedSlots == 1)
		return ClientSessionMasterUpdate::FirstConnectedClient;

	if (maxSlots > 0 && connectedSlots == maxSlots)
		return ClientSessionMasterUpdate::FullServer;

	return ClientSessionMasterUpdate::None;
}

ClientSessionMasterUpdate BuildClientSessionDropMasterUpdate(
	int connectedSlots)
{
	if (connectedSlots == 0)
		return ClientSessionMasterUpdate::EmptyServer;

	return ClientSessionMasterUpdate::None;
}

}
}
}
