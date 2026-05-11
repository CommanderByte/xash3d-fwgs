#include "server_resource_message_adapter.h"

#include "engine/server/messaging/server_resource_message.hpp"
#include "resource_adapter_shared.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_resource_message_write_result_t SV_ResourceMessage_WriteResource(
	const resource_t *resource,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);

	xash::engine::server::WriteResourceMessageRow(
		buffer,
		xash::engine::server::adapter::ToModernResourceMessageRow(resource));

	return xash::engine::server::adapter::MakeWriteResult<
		sv_resource_message_write_result_t>(buffer);
}
