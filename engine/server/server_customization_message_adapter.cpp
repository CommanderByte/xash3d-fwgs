#include "server_customization_message_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_customization_message.hpp"
#include "resource_adapter_shared.hpp"

extern "C" sv_customization_message_write_result_t SV_CustomizationMessage_WritePayload(
	const resource_t *resource,
	int playernum,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));

	xash::engine::server::WriteCustomizationMessagePayload(
		buffer,
		xash::engine::server::adapter::ToModernCustomizationMessage(
			resource,
			playernum));

	sv_customization_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}
