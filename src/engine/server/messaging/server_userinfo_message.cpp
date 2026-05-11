#include "engine/server/messaging/server_userinfo_message.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

void WriteString(xash::engine::network::NetworkBitBuffer &buffer, const char *value)
{
	if (!value)
		value = "";

	const std::size_t length = std::strlen(value);
	for (std::size_t i = 0; i <= length; ++i)
		buffer.writeUnsigned(static_cast<std::uint8_t>(value[i]), 8);
}

void WriteDigest(
	xash::engine::network::NetworkBitBuffer &buffer,
	const std::uint8_t *digest)
{
	static const std::uint8_t zeros[kUserinfoDigestSize] = {};
	const std::uint8_t *bytes = digest ? digest : zeros;

	for (std::size_t i = 0; i < kUserinfoDigestSize; ++i)
		buffer.writeUnsigned(bytes[i], 8);
}

}

void WriteUserinfoUpdatePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const UserinfoUpdatePayload &payload)
{
	buffer.writeUnsigned(static_cast<std::uint32_t>(payload.clientIndex), kUserinfoClientIndexBits);
	buffer.writeSigned(payload.userId, 32);
	buffer.writeOneBit(payload.active ? 1 : 0);

	if (!payload.active)
		return;

	WriteString(buffer, payload.userinfo);
	WriteDigest(buffer, payload.hashedCdKeyDigest);
}

}
}
}
