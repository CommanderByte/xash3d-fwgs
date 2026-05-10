#ifndef XASH_ENGINE_SERVER_FRAME_DATAGRAM_ADAPTER_H
#define XASH_ENGINE_SERVER_FRAME_DATAGRAM_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_frame_transfer_action_e
{
	SV_FRAME_TRANSFER_NONE = 0,
	SV_FRAME_TRANSFER_COPY = 1,
	SV_FRAME_TRANSFER_FRAGMENT = 2,
	SV_FRAME_TRANSFER_IGNORE = 3,
	SV_FRAME_TRANSFER_SOURCE_OVERFLOW = 4,
	SV_FRAME_TRANSFER_CLEAR_OVERFLOW = 5
};

typedef struct sv_frame_transfer_plan_s
{
	enum sv_frame_transfer_action_e action;
	int clear_source;
	int warn;
	int update_overflow_warn_time;
	double next_overflow_warn_time;
} sv_frame_transfer_plan_t;

typedef struct sv_frame_reliable_resend_plan_s
{
	int send_userinfo;
	int clear_userinfo_flag;
	int update_next_sendinfo_time;
	double next_sendinfo_time;
	int send_movevars;
	int clear_movevars_flag;
} sv_frame_reliable_resend_plan_t;

sv_frame_transfer_plan_t SV_Frame_BuildClientDatagramAppendPlan(
	int source_overflowed,
	int source_bytes_written,
	int destination_bytes_left,
	double realtime,
	double overflow_warn_time);
sv_frame_transfer_plan_t SV_Frame_BuildServerReliableDatagramPlan(
	int source_bytes_written,
	int destination_bytes_left);
sv_frame_transfer_plan_t SV_Frame_BuildServerUnreliableDatagramPlan(
	int source_bytes_written,
	int destination_bytes_left);
sv_frame_transfer_plan_t SV_Frame_BuildServerSpectatorDatagramPlan(
	int hltv_proxy,
	int source_bytes_written,
	int destination_bytes_left);
sv_frame_transfer_plan_t SV_Frame_BuildOverflowClearPlan(int overflowed);

sv_frame_reliable_resend_plan_t SV_Frame_BuildReliableResendPlan(
	int flags,
	double next_sendinfo_time,
	double realtime,
	int reliable_bytes_left,
	int userinfo_length);

int SV_Frame_ShouldProcessClient(int client_state, int fake_client);
int SV_Frame_ShouldClearSkipNetMessage(int flags);
int SV_Frame_ShouldForceLocalClientSend(int limit_local, int local_address);
int SV_Frame_ShouldScheduleSpawnedClientMessage(
	int client_state,
	double next_message_time,
	double realtime,
	double frame_time);
int SV_Frame_ShouldDropReliableOverflow(int overflowed);
int SV_Frame_ShouldClearSendAfterFailureTimeout(
	int flags,
	double failure_time,
	double realtime,
	double last_received);
int SV_Frame_ShouldSendClientFrame(int flags);

#ifdef __cplusplus
}
#endif

#endif
