#include "server_userinfo_message_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_userinfo_message.hpp"

#include <cstddef>

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
	xash::engine::network::NetworkBitBuffer buffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));

	xash::engine::server::UserinfoUpdatePayload payload = {};
	payload.clientIndex = client_index;
	payload.userId = user_id;
	payload.active = active != 0;
	payload.userinfo = userinfo;
	payload.hashedCdKeyDigest = digest;

	xash::engine::server::WriteUserinfoUpdatePayload(buffer, payload);

	sv_userinfo_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}
