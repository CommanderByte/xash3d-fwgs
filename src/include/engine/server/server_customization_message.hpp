#ifndef XASH_ENGINE_SERVER_SERVER_CUSTOMIZATION_MESSAGE_HPP
#define XASH_ENGINE_SERVER_SERVER_CUSTOMIZATION_MESSAGE_HPP

#include "engine/network/network_buffer.hpp"
#include "engine/server/resource_identity.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::size_t kCustomizationMessageHashSize = 16;
constexpr unsigned int kCustomizationMessageFlagCustom = 1u << 2;

struct CustomizationMessage
{
	int playerNumber;
	ResourceType type;
	const char *name;
	int index;
	int downloadSize;
	unsigned int flags;
	const std::uint8_t *md5Hash;
};

void WriteCustomizationMessagePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const CustomizationMessage &message);

}
}
}

#endif
