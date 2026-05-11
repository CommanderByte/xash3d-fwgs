#ifndef XASH_ENGINE_SERVER_PACKET_ENTITIES_DELTA_ADAPTER_H
#define XASH_ENGINE_SERVER_PACKET_ENTITIES_DELTA_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_packet_entity_header_action_e
{
	SV_PACKET_ENTITY_HEADER_FULL = 0,
	SV_PACKET_ENTITY_HEADER_DELTA = 1
};

enum sv_packet_entity_cursor_action_e
{
	SV_PACKET_ENTITY_CURSOR_FINISH = 0,
	SV_PACKET_ENTITY_CURSOR_DELTA_FROM_OLD = 1,
	SV_PACKET_ENTITY_CURSOR_ADD_FROM_BASELINE = 2,
	SV_PACKET_ENTITY_CURSOR_REMOVE_FROM_OLD = 3
};

typedef struct sv_packet_entity_header_plan_s
{
	enum sv_packet_entity_header_action_e action;
	int use_previous_frame;
	int warn_outdated_delta;
	int old_entity_count;
} sv_packet_entity_header_plan_t;

typedef struct sv_packet_entity_cursor_plan_s
{
	enum sv_packet_entity_cursor_action_e action;
	int advance_new;
	int advance_old;
} sv_packet_entity_cursor_plan_t;

sv_packet_entity_header_plan_t SV_PacketEntities_BuildHeaderPlan(
	int has_delta_sequence,
	int old_frame_first_entity,
	int next_client_entity,
	int client_entity_capacity,
	int old_entity_count);

sv_packet_entity_cursor_plan_t SV_PacketEntities_BuildCursorPlan(
	int new_index,
	int new_count,
	int new_number,
	int old_index,
	int old_count,
	int old_number,
	int end_number);

#ifdef __cplusplus
}
#endif

#endif
