#include "server_consistency_list_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/resources/server_consistency_list.hpp"
#include "resource_adapter_shared.hpp"
#include "server_message_adapter_shared.hpp"

#include <cstddef>
#include <vector>

extern "C" sv_consistency_list_write_result_t SV_ConsistencyList_Write(
	const resource_t *resources,
	int resource_count,
	int consistency_count,
	int max_clients,
	int consistency_enabled,
	int hltv_proxy,
	int already_overflow,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	const std::vector<int> indexes =
		xash::engine::server::adapter::BuildCheckedResourceIndexSnapshot(
			resources,
			resource_count);

	xash::engine::server::ConsistencyListRequest request = {};
	request.maxClients = max_clients;
	request.consistencyEnabled = consistency_enabled != 0;
	request.consistencyCount = consistency_count;
	request.hltvProxy = hltv_proxy != 0;
	request.resourceIndexes = indexes.empty() ? nullptr : indexes.data();
	request.resourceIndexCount = indexes.size();

	sv_consistency_list_write_result_t legacy = {};
	legacy.current_bit = current_bit;
	legacy.overflow = already_overflow ? 1 : 0;
	legacy.force_unmodified =
		xash::engine::server::ConsistencyListShouldSend(request) ? 1 : 0;

	if (already_overflow)
		return legacy;

	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);

	const xash::engine::server::ConsistencyListWriteResult result =
		xash::engine::server::WriteConsistencyList(buffer, request);

	legacy.current_bit = static_cast<int>(buffer.tellBit());
	legacy.overflow = buffer.overflow() ? 1 : 0;
	legacy.force_unmodified = result.forceUnmodified ? 1 : 0;
	return legacy;
}
