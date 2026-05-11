#include "server_packet_entities_delta_adapter.h"

#include "engine/server/messaging/server_packet_entities_delta.hpp"

static_assert(SV_PACKET_ENTITY_HEADER_FULL ==
	static_cast<int>(xash::engine::server::PacketEntityHeaderAction::Full),
	"packet entity full-header action changed");
static_assert(SV_PACKET_ENTITY_HEADER_DELTA ==
	static_cast<int>(xash::engine::server::PacketEntityHeaderAction::Delta),
	"packet entity delta-header action changed");
static_assert(SV_PACKET_ENTITY_CURSOR_FINISH ==
	static_cast<int>(xash::engine::server::PacketEntityCursorAction::Finish),
	"packet entity finish cursor action changed");
static_assert(SV_PACKET_ENTITY_CURSOR_DELTA_FROM_OLD ==
	static_cast<int>(xash::engine::server::PacketEntityCursorAction::DeltaFromOld),
	"packet entity delta-from-old cursor action changed");
static_assert(SV_PACKET_ENTITY_CURSOR_ADD_FROM_BASELINE ==
	static_cast<int>(xash::engine::server::PacketEntityCursorAction::AddFromBaseline),
	"packet entity add-from-baseline cursor action changed");
static_assert(SV_PACKET_ENTITY_CURSOR_REMOVE_FROM_OLD ==
	static_cast<int>(xash::engine::server::PacketEntityCursorAction::RemoveFromOld),
	"packet entity remove-from-old cursor action changed");

extern "C" sv_packet_entity_header_plan_t SV_PacketEntities_BuildHeaderPlan(
	int has_delta_sequence,
	int old_frame_first_entity,
	int next_client_entity,
	int client_entity_capacity,
	int old_entity_count)
{
	const xash::engine::server::PacketEntityHeaderPlan plan =
		xash::engine::server::BuildPacketEntityHeaderPlan(
			has_delta_sequence != 0,
			old_frame_first_entity,
			next_client_entity,
			client_entity_capacity,
			old_entity_count);

	sv_packet_entity_header_plan_t legacy = {};
	legacy.action = static_cast<sv_packet_entity_header_action_e>(plan.action);
	legacy.use_previous_frame = plan.usePreviousFrame ? 1 : 0;
	legacy.warn_outdated_delta = plan.warnOutdatedDelta ? 1 : 0;
	legacy.old_entity_count = plan.oldEntityCount;
	return legacy;
}

extern "C" sv_packet_entity_cursor_plan_t SV_PacketEntities_BuildCursorPlan(
	int new_index,
	int new_count,
	int new_number,
	int old_index,
	int old_count,
	int old_number,
	int end_number)
{
	const xash::engine::server::PacketEntityCursorPlan plan =
		xash::engine::server::BuildPacketEntityCursorPlan(
			new_index,
			new_count,
			new_number,
			old_index,
			old_count,
			old_number,
			end_number);

	sv_packet_entity_cursor_plan_t legacy = {};
	legacy.action = static_cast<sv_packet_entity_cursor_action_e>(plan.action);
	legacy.advance_new = plan.advanceNew;
	legacy.advance_old = plan.advanceOld;
	return legacy;
}
