#include "client_session_slots_adapter.h"

#include <cstddef>

#include "engine/server/client_session_slots.hpp"

static_assert(SV_CLIENT_SESSION_SLOT_FREE ==
	xash::engine::server::kClientSessionSlotFree,
	"free client slot state changed");
static_assert(SV_CLIENT_SESSION_SLOT_ZOMBIE ==
	xash::engine::server::kClientSessionSlotZombie,
	"zombie client slot state changed");
static_assert(SV_CLIENT_SESSION_SLOT_CONNECTED ==
	xash::engine::server::kClientSessionSlotConnected,
	"connected client slot state changed");
static_assert(SV_CLIENT_SESSION_SLOT_SPAWNING ==
	xash::engine::server::kClientSessionSlotSpawning,
	"spawning client slot state changed");
static_assert(SV_CLIENT_SESSION_SLOT_SPAWNED ==
	xash::engine::server::kClientSessionSlotSpawned,
	"spawned client slot state changed");
static_assert(sizeof(sv_client_session_slot_snapshot_t) ==
	sizeof(xash::engine::server::ClientSessionSlotSnapshot),
	"client session slot snapshot layout changed");
static_assert(offsetof(sv_client_session_slot_snapshot_t, state) ==
	offsetof(xash::engine::server::ClientSessionSlotSnapshot, state),
	"client session slot state offset changed");
static_assert(offsetof(sv_client_session_slot_snapshot_t, flags) ==
	offsetof(xash::engine::server::ClientSessionSlotSnapshot, flags),
	"client session slot flags offset changed");

namespace
{

int ToLegacyMasterUpdate(
	xash::engine::server::ClientSessionMasterUpdate update)
{
	return static_cast<int>(update);
}

}

extern "C" int SV_ClientSession_SlotIsFree(int state)
{
	return xash::engine::server::ClientSessionSlotIsFree(state) ? 1 : 0;
}

extern "C" int SV_ClientSession_SlotIsConnected(int state)
{
	return xash::engine::server::ClientSessionSlotIsConnected(state) ? 1 : 0;
}

extern "C" int SV_ClientSession_SlotIsFakeClient(unsigned int flags)
{
	return xash::engine::server::ClientSessionSlotIsFakeClient(flags) ? 1 : 0;
}

extern "C" sv_client_session_population_t SV_ClientSession_CountPopulation(
	const sv_client_session_slot_snapshot_t *slots,
	int slot_count)
{
	const xash::engine::server::ClientSessionPopulation modern =
		xash::engine::server::CountClientSessionPopulation(
			reinterpret_cast<
				const xash::engine::server::ClientSessionSlotSnapshot *>(
				slots),
			slot_count);

	sv_client_session_population_t legacy = {};
	legacy.players = modern.players;
	legacy.bots = modern.bots;
	legacy.connected = modern.connected;
	return legacy;
}

extern "C" int SV_ClientSession_FindFirstFreeSlot(
	const sv_client_session_slot_snapshot_t *slots,
	int slot_count)
{
	return xash::engine::server::FindFirstFreeClientSessionSlot(
		reinterpret_cast<
			const xash::engine::server::ClientSessionSlotSnapshot *>(slots),
		slot_count);
}

extern "C" int SV_ClientSession_BuildConnectMasterUpdate(
	int connected_slots,
	int max_slots)
{
	return ToLegacyMasterUpdate(
		xash::engine::server::BuildClientSessionConnectMasterUpdate(
			connected_slots,
			max_slots));
}

extern "C" int SV_ClientSession_BuildDropMasterUpdate(int connected_slots)
{
	return ToLegacyMasterUpdate(
		xash::engine::server::BuildClientSessionDropMasterUpdate(
			connected_slots));
}
