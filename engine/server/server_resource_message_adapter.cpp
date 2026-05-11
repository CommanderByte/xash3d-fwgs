#include "server_resource_message_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_resource_message.hpp"
#include "resource_adapter_shared.hpp"

extern "C" sv_resource_message_write_result_t SV_ResourceMessage_WriteResource(
	const resource_t *resource,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));

	xash::engine::server::WriteResourceMessageRow(
		buffer,
		xash::engine::server::adapter::ToModernResourceMessageRow(resource));

	sv_resource_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}
