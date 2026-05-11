#include "engine/server/game_dll/game_dll_message_bridge.hpp"

#include "engine/server/messaging/server_message_envelope.hpp"

namespace xash
{
namespace engine
{
namespace server
{

GameDllMessageBeginRequest BuildGameDllUserMessageBeginRequest(
	int destination,
	const GameDllUserMessageSlot &slot)
{
	GameDllMessageBeginRequest request = {};
	request.destination = destination;
	request.messageNumber = slot.number;
	request.name = slot.name;
	request.payloadSize = slot.size;
	request.systemMessage = false;
	request.rewriteEnabled = false;
	request.rewriteTargetMessage = 0;
	request.rewriteName = nullptr;
	return request;
}

GameDllUserMessageRegistrationBroadcast
BuildGameDllUserMessageRegistrationBroadcast(
	const GameDllUserMessageRegistrationPlan &plan,
	const char *name)
{
	GameDllUserMessageRegistrationBroadcast broadcast = {};

	if (plan.action != GameDllUserMessageRegistrationAction::RegisterNew ||
		!plan.resendRegistration)
	{
		return broadcast;
	}

	broadcast.shouldWrite = true;
	broadcast.messageNumber = plan.messageNumber;
	broadcast.payloadSize = plan.storedSize;
	broadcast.name = name;
	return broadcast;
}

void WriteGameDllUserMessageRegistrationBroadcast(
	xash::engine::network::NetworkBitBuffer &buffer,
	const GameDllUserMessageRegistrationBroadcast &broadcast)
{
	if (!broadcast.shouldWrite)
		return;

	WriteServerMessageCommand(
		buffer,
		kGameDllUserMessageRegistrationCommand);
	WriteServerMessageByte(
		buffer,
		static_cast<unsigned int>(broadcast.messageNumber));
	buffer.writeUnsigned(
		static_cast<std::uint16_t>(broadcast.payloadSize),
		16);
	WriteServerMessageString(buffer, broadcast.name);
}

}
}
}
