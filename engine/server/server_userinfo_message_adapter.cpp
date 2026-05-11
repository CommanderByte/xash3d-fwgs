#include "server_userinfo_message_adapter.h"

#include "engine/server/messaging/server_userinfo_message.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_userinfo_message_write_result_t SV_UserinfoMessage_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int client_index,
	int user_id,
	int active,
	const char *userinfo,
	const unsigned char digest[16])
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);

	xash::engine::server::UserinfoUpdatePayload payload = {};
	payload.clientIndex = client_index;
	payload.userId = user_id;
	payload.active = active != 0;
	payload.userinfo = userinfo;
	payload.hashedCdKeyDigest = digest;

	xash::engine::server::WriteUserinfoUpdatePayload(buffer, payload);

	return xash::engine::server::adapter::MakeWriteResult<
		sv_userinfo_message_write_result_t>(buffer);
}
