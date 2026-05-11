#ifndef XASH_ENGINE_SERVER_SERVER_MESSAGE_ADAPTER_SHARED_HPP
#define XASH_ENGINE_SERVER_SERVER_MESSAGE_ADAPTER_SHARED_HPP

#include "engine/network/network_buffer.hpp"

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{
namespace adapter
{

inline xash::engine::network::NetworkBitBuffer MakeNetworkBitBuffer(
	unsigned char *data,
	int dataBits,
	int currentBit)
{
	return xash::engine::network::NetworkBitBuffer(
		data,
		dataBits < 0 ? 0U : static_cast<std::size_t>(dataBits),
		currentBit < 0 ? 0U : static_cast<std::size_t>(currentBit));
}

template <typename WriteResult>
WriteResult MakeWriteResult(
	const xash::engine::network::NetworkBitBuffer &buffer)
{
	WriteResult result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}

}
}
}
}

#endif
