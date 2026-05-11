#pragma once

#include <cstddef>

#include "engine/network/network_buffer.hpp"
#include "engine/server/game_dll/game_dll_user_message_registry.hpp"

namespace xash
{
namespace tests
{
namespace engine
{

inline xash::engine::server::GameDllUserMessageSlot Slot(
	const char *name,
	int number,
	int size)
{
	xash::engine::server::GameDllUserMessageSlot slot = {};
	slot.name = name;
	slot.number = number;
	slot.size = size;
	return slot;
}

inline xash::engine::server::GameDllUserMessageRegistrationRequest
RegistrationRequest(
	const xash::engine::server::GameDllUserMessageSlot *slots,
	int slotCount,
	const char *name,
	int size,
	bool serverActive = false)
{
	xash::engine::server::GameDllUserMessageRegistrationRequest request = {};
	request.name = name;
	request.requestedSize = size;
	request.slots = slots;
	request.slotCount = slotCount;
	request.nameCapacity =
		xash::engine::server::kGameDllUserMessageNameCapacity;
	request.serverActive = serverActive;
	return request;
}

inline bool ReadCString(
	xash::engine::network::NetworkBitBuffer &reader,
	const char *expected)
{
	for (std::size_t i = 0; ; ++i)
	{
		const unsigned int value = reader.readUnsigned(8);
		if (value != static_cast<unsigned char>(expected[i]))
			return false;

		if (value == 0)
			return true;
	}
}

}
}
}
