#include "server_frame_datagram_adapter.h"

#include "engine/server/messaging/server_frame_datagram.hpp"

namespace
{

sv_frame_transfer_plan_t ToLegacyTransferPlan(
	const xash::engine::server::FrameTransferPlan &plan)
{
	sv_frame_transfer_plan_t legacy = {};
	legacy.action = static_cast<sv_frame_transfer_action_e>(plan.action);
	legacy.clear_source = plan.clearSource ? 1 : 0;
	legacy.warn = plan.warn ? 1 : 0;
	legacy.update_overflow_warn_time =
		plan.updateOverflowWarnTime ? 1 : 0;
	legacy.next_overflow_warn_time = plan.nextOverflowWarnTime;
	return legacy;
}

sv_frame_reliable_resend_plan_t ToLegacyResendPlan(
	const xash::engine::server::FrameReliableResendPlan &plan)
{
	sv_frame_reliable_resend_plan_t legacy = {};
	legacy.send_userinfo = plan.sendUserinfo ? 1 : 0;
	legacy.clear_userinfo_flag = plan.clearUserinfoFlag ? 1 : 0;
	legacy.update_next_sendinfo_time = plan.updateNextSendInfoTime ? 1 : 0;
	legacy.next_sendinfo_time = plan.nextSendInfoTime;
	legacy.send_movevars = plan.sendMovevars ? 1 : 0;
	legacy.clear_movevars_flag = plan.clearMovevarsFlag ? 1 : 0;
	return legacy;
}

}

extern "C" sv_frame_transfer_plan_t SV_Frame_BuildClientDatagramAppendPlan(
	int source_overflowed,
	int source_bytes_written,
	int destination_bytes_left,
	double realtime,
	double overflow_warn_time)
{
	return ToLegacyTransferPlan(
		xash::engine::server::BuildClientDatagramAppendPlan(
			source_overflowed != 0,
			source_bytes_written,
			destination_bytes_left,
			realtime,
			overflow_warn_time));
}

extern "C" sv_frame_transfer_plan_t SV_Frame_BuildServerReliableDatagramPlan(
	int source_bytes_written,
	int destination_bytes_left)
{
	return ToLegacyTransferPlan(
		xash::engine::server::BuildServerReliableDatagramPlan(
			source_bytes_written,
			destination_bytes_left));
}

extern "C" sv_frame_transfer_plan_t SV_Frame_BuildServerUnreliableDatagramPlan(
	int source_bytes_written,
	int destination_bytes_left)
{
	return ToLegacyTransferPlan(
		xash::engine::server::BuildServerUnreliableDatagramPlan(
			source_bytes_written,
			destination_bytes_left));
}

extern "C" sv_frame_transfer_plan_t SV_Frame_BuildServerSpectatorDatagramPlan(
	int hltv_proxy,
	int source_bytes_written,
	int destination_bytes_left)
{
	return ToLegacyTransferPlan(
		xash::engine::server::BuildServerSpectatorDatagramPlan(
			hltv_proxy != 0,
			source_bytes_written,
			destination_bytes_left));
}

extern "C" sv_frame_transfer_plan_t SV_Frame_BuildOverflowClearPlan(
	int overflowed)
{
	return ToLegacyTransferPlan(
		xash::engine::server::BuildOverflowClearPlan(overflowed != 0));
}

extern "C" sv_frame_reliable_resend_plan_t SV_Frame_BuildReliableResendPlan(
	int flags,
	double next_sendinfo_time,
	double realtime,
	int reliable_bytes_left,
	int userinfo_length)
{
	return ToLegacyResendPlan(
		xash::engine::server::BuildReliableResendPlan(
			flags,
			next_sendinfo_time,
			realtime,
			reliable_bytes_left,
			userinfo_length));
}

extern "C" int SV_Frame_ShouldProcessClient(
	int client_state,
	int fake_client)
{
	return xash::engine::server::ShouldProcessFrameClient(
		client_state,
		fake_client != 0) ? 1 : 0;
}

extern "C" int SV_Frame_ShouldClearSkipNetMessage(int flags)
{
	return xash::engine::server::ShouldClearSkipNetMessage(flags) ? 1 : 0;
}

extern "C" int SV_Frame_ShouldForceLocalClientSend(
	int limit_local,
	int local_address)
{
	return xash::engine::server::ShouldForceLocalClientSend(
		limit_local != 0,
		local_address != 0) ? 1 : 0;
}

extern "C" int SV_Frame_ShouldScheduleSpawnedClientMessage(
	int client_state,
	double next_message_time,
	double realtime,
	double frame_time)
{
	return xash::engine::server::ShouldScheduleSpawnedClientMessage(
		client_state,
		next_message_time,
		realtime,
		frame_time) ? 1 : 0;
}

extern "C" int SV_Frame_ShouldDropReliableOverflow(int overflowed)
{
	return xash::engine::server::ShouldDropReliableOverflow(
		overflowed != 0) ? 1 : 0;
}

extern "C" int SV_Frame_ShouldClearSendAfterFailureTimeout(
	int flags,
	double failure_time,
	double realtime,
	double last_received)
{
	return xash::engine::server::ShouldClearSendAfterFailureTimeout(
		flags,
		failure_time,
		realtime,
		last_received) ? 1 : 0;
}

extern "C" int SV_Frame_ShouldSendClientFrame(int flags)
{
	return xash::engine::server::ShouldSendClientFrame(flags) ? 1 : 0;
}
