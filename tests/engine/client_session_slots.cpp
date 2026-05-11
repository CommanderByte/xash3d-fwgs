#include <cstdlib>

#include "engine/server/client/client_session_slots.hpp"
#include "engine/server/shared/server_limits.hpp"

using namespace xash::engine::server;

namespace
{

bool TestPopulationCountsConnectedHumansAndBots()
{
	const ClientSessionSlotSnapshot slots[] = {
		{ kClientSessionSlotFree, 0u },
		{ kClientSessionSlotZombie, kServerClientFlagFakeClient },
		{ kClientSessionSlotConnected, 0u },
		{ kClientSessionSlotSpawning, kServerClientFlagFakeClient },
		{ kClientSessionSlotSpawned, 0u },
	};

	const ClientSessionPopulation population =
		CountClientSessionPopulation(slots, 5);

	return population.connected == 3 &&
		population.players == 2 &&
		population.bots == 1;
}

bool TestPopulationIgnoresInvalidInput()
{
	const ClientSessionPopulation nullPopulation =
		CountClientSessionPopulation(nullptr, 3);
	const ClientSessionSlotSnapshot slot = {
		kClientSessionSlotConnected,
		0u,
	};
	const ClientSessionPopulation negativePopulation =
		CountClientSessionPopulation(&slot, -1);

	return nullPopulation.connected == 0 &&
		nullPopulation.players == 0 &&
		nullPopulation.bots == 0 &&
		negativePopulation.connected == 0 &&
		negativePopulation.players == 0 &&
		negativePopulation.bots == 0;
}

bool TestFirstFreeSlotSkipsZombieAndConnectedSlots()
{
	const ClientSessionSlotSnapshot slots[] = {
		{ kClientSessionSlotZombie, 0u },
		{ kClientSessionSlotConnected, 0u },
		{ kClientSessionSlotFree, 0u },
		{ kClientSessionSlotFree, 0u },
	};

	return FindFirstFreeClientSessionSlot(slots, 4) == 2;
}

bool TestFirstFreeSlotReportsMissing()
{
	const ClientSessionSlotSnapshot slots[] = {
		{ kClientSessionSlotZombie, 0u },
		{ kClientSessionSlotConnected, 0u },
		{ kClientSessionSlotSpawned, kServerClientFlagFakeClient },
	};

	return FindFirstFreeClientSessionSlot(slots, 3) == -1 &&
		FindFirstFreeClientSessionSlot(nullptr, 3) == -1 &&
		FindFirstFreeClientSessionSlot(slots, 0) == -1;
}

bool TestConnectMasterUpdateReasons()
{
	return BuildClientSessionConnectMasterUpdate(1, 8) ==
			ClientSessionMasterUpdate::FirstConnectedClient &&
		BuildClientSessionConnectMasterUpdate(8, 8) ==
			ClientSessionMasterUpdate::FullServer &&
		BuildClientSessionConnectMasterUpdate(3, 8) ==
			ClientSessionMasterUpdate::None &&
		BuildClientSessionConnectMasterUpdate(0, 0) ==
			ClientSessionMasterUpdate::None;
}

bool TestDropMasterUpdateReasons()
{
	return BuildClientSessionDropMasterUpdate(0) ==
			ClientSessionMasterUpdate::EmptyServer &&
		BuildClientSessionDropMasterUpdate(1) ==
			ClientSessionMasterUpdate::None;
}

}

int main()
{
	if (!TestPopulationCountsConnectedHumansAndBots() ||
		!TestPopulationIgnoresInvalidInput() ||
		!TestFirstFreeSlotSkipsZombieAndConnectedSlots() ||
		!TestFirstFreeSlotReportsMissing() ||
		!TestConnectMasterUpdateReasons() ||
		!TestDropMasterUpdateReasons())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
