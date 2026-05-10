#include "engine/server/server_consistency_list.hpp"

namespace xash
{
namespace engine
{
namespace server
{

bool ConsistencyListShouldSend(const ConsistencyListRequest &request)
{
	return request.maxClients != 1 &&
		request.consistencyEnabled &&
		request.consistencyCount != 0 &&
		!request.hltvProxy;
}

ConsistencyListWriteResult WriteConsistencyList(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ConsistencyListRequest &request)
{
	ConsistencyListWriteResult result = {};
	result.forceUnmodified = ConsistencyListShouldSend(request);

	if (!result.forceUnmodified)
	{
		buffer.writeOneBit(0);
		return result;
	}

	buffer.writeOneBit(1);

	int lastCheck = 0;
	for (std::size_t i = 0; i < request.resourceIndexCount; ++i)
	{
		const int index = request.resourceIndexes[i];
		const int delta = index - lastCheck;

		buffer.writeOneBit(1);

		if (delta > kConsistencyListMaxDelta)
		{
			buffer.writeOneBit(0);
			buffer.writeUnsigned(static_cast<unsigned int>(index), kConsistencyListResourceIndexBits);
		}
		else
		{
			buffer.writeOneBit(1);
			buffer.writeUnsigned(static_cast<unsigned int>(delta), kConsistencyListDeltaBits);
		}

		lastCheck = index;
	}

	buffer.writeOneBit(0);
	return result;
}

}
}
}
