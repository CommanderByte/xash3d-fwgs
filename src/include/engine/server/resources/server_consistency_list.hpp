#ifndef XASH_ENGINE_SERVER_SERVER_CONSISTENCY_LIST_HPP
#define XASH_ENGINE_SERVER_SERVER_CONSISTENCY_LIST_HPP

#include "engine/network/network_buffer.hpp"

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kConsistencyListResourceIndexBits = 12;
constexpr int kConsistencyListDeltaBits = 5;
constexpr int kConsistencyListMaxDelta = 31;

struct ConsistencyListRequest
{
	int maxClients;
	bool consistencyEnabled;
	int consistencyCount;
	bool hltvProxy;
	const int *resourceIndexes;
	std::size_t resourceIndexCount;
};

struct ConsistencyListWriteResult
{
	bool forceUnmodified;
};

bool ConsistencyListShouldSend(const ConsistencyListRequest &request);
ConsistencyListWriteResult WriteConsistencyList(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ConsistencyListRequest &request);

}
}
}

#endif
