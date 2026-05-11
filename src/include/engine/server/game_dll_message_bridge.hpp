#ifndef XASH_ENGINE_SERVER_GAME_DLL_MESSAGE_BRIDGE_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_MESSAGE_BRIDGE_HPP

#include "engine/network/network_buffer.hpp"
#include "engine/server/game_dll_message_session.hpp"
#include "engine/server/game_dll_user_message_registry.hpp"

#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint8_t kGameDllUserMessageRegistrationCommand = 39;

struct GameDllUserMessageRegistrationBroadcast
{
	bool shouldWrite;
	int messageNumber;
	int payloadSize;
	const char *name;
};

GameDllMessageBeginRequest BuildGameDllUserMessageBeginRequest(
	int destination,
	const GameDllUserMessageSlot &slot);

GameDllUserMessageRegistrationBroadcast
BuildGameDllUserMessageRegistrationBroadcast(
	const GameDllUserMessageRegistrationPlan &plan,
	const char *name);

void WriteGameDllUserMessageRegistrationBroadcast(
	xash::engine::network::NetworkBitBuffer &buffer,
	const GameDllUserMessageRegistrationBroadcast &broadcast);

}
}
}

#endif
