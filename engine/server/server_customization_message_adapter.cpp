#include "server_customization_message_adapter.h"

#include "engine/server/server_customization_message.hpp"
#include "resource_adapter_shared.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_customization_message_write_result_t SV_CustomizationMessage_WritePayload(
	const resource_t *resource,
	int playernum,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);

	xash::engine::server::WriteCustomizationMessagePayload(
		buffer,
		xash::engine::server::adapter::ToModernCustomizationMessage(
			resource,
			playernum));

	return xash::engine::server::adapter::MakeWriteResult<
		sv_customization_message_write_result_t>(buffer);
}
