#ifndef XASH_ENGINE_SERVER_SERVER_USERINFO_MESSAGE_HPP
#define XASH_ENGINE_SERVER_SERVER_USERINFO_MESSAGE_HPP

#include "engine/network/network_buffer.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kUserinfoClientIndexBits = 5;
constexpr std::size_t kUserinfoDigestSize = 16;

struct UserinfoUpdatePayload
{
	int clientIndex;
	int userId;
	bool active;
	const char *userinfo;
	const std::uint8_t *hashedCdKeyDigest;
};

void WriteUserinfoUpdatePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const UserinfoUpdatePayload &payload);

}
}
}

#endif
