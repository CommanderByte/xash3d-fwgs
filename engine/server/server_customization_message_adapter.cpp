#include "server_customization_message_adapter.h"

#include "engine/server/messaging/server_customization_message.hpp"
#include "resource_adapter_shared.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_customization_message_write_result_t SV_CustomizationMessage_WritePayload(
	const resource_t *resource,
	int playernum,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	const xash::engine::server::CustomizationMessage message =
		xash::engine::server::adapter::ToModernCustomizationMessage(
			resource,
			playernum);

	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_customization_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteCustomizationMessagePayload,
			message);
}
