#include "engine/server/game_dll/game_dll_user_message_registry.hpp"
#include "engine/server/game_dll/game_dll_message_bridge.hpp"

#include "engine/server/messaging/server_message_envelope.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool SlotOccupied(const GameDllUserMessageSlot &slot)
{
	return slot.name && slot.name[0];
}

GameDllUserMessageRegistrationPlan Reject(
	GameDllUserMessageRejectReason reason)
{
	GameDllUserMessageRegistrationPlan plan = {};
	plan.action = GameDllUserMessageRegistrationAction::Reject;
	plan.rejectReason = reason;
	plan.messageNumber = kGameDllUserMessageBadMessage;
	plan.slotIndex = -1;
	return plan;
}

}

bool IsGameDllUserMessageNameEmpty(const char *name)
{
	return !name || !name[0];
}

bool IsGameDllUserMessageNameTooLong(const char *name, int nameCapacity)
{
	if (!name || nameCapacity <= 0)
		return false;

	return static_cast<int>(std::strlen(name)) >= nameCapacity;
}

int ClampGameDllUserMessageSize(int size)
{
	if (size < -1)
		return -1;

	if (size > kGameDllUserMessageMaxPayloadBytes)
		return kGameDllUserMessageMaxPayloadBytes;

	return size;
}

GameDllUserMessageRegistrationPlan BuildGameDllUserMessageRegistrationPlan(
	const GameDllUserMessageRegistrationRequest &request)
{
	if (IsGameDllUserMessageNameEmpty(request.name))
		return Reject(GameDllUserMessageRejectReason::EmptyName);

	if (IsGameDllUserMessageNameTooLong(
		request.name,
		request.nameCapacity))
	{
		return Reject(GameDllUserMessageRejectReason::NameTooLong);
	}

	if (request.requestedSize > kGameDllUserMessageMaxPayloadBytes)
		return Reject(GameDllUserMessageRejectReason::SizeTooLarge);

	if (!request.slots ||
		request.slotCount <= kGameDllUserMessageFirstSlot)
	{
		return Reject(GameDllUserMessageRejectReason::CapacityExceeded);
	}

	int slotIndex = kGameDllUserMessageFirstSlot;
	for (; slotIndex < request.slotCount; ++slotIndex)
	{
		const GameDllUserMessageSlot &slot = request.slots[slotIndex];
		if (!SlotOccupied(slot))
			break;

		if (std::strcmp(slot.name, request.name) == 0)
		{
			GameDllUserMessageRegistrationPlan plan = {};
			plan.action =
				GameDllUserMessageRegistrationAction::ReturnExisting;
			plan.slotIndex = slotIndex;
			plan.messageNumber = slot.number;
			plan.storedSize = slot.size;
			return plan;
		}
	}

	if (slotIndex == request.slotCount)
		return Reject(GameDllUserMessageRejectReason::CapacityExceeded);

	GameDllUserMessageRegistrationPlan plan = {};
	plan.action = GameDllUserMessageRegistrationAction::RegisterNew;
	plan.slotIndex = slotIndex;
	plan.messageNumber =
		kGameDllUserMessageLastServiceMessage + slotIndex;
	plan.storedSize = ClampGameDllUserMessageSize(request.requestedSize);
	plan.resendRegistration = request.serverActive;
	return plan;
}

const char *GameDllUserMessageRegistrationActionName(
	GameDllUserMessageRegistrationAction action)
{
	switch (action)
	{
	case GameDllUserMessageRegistrationAction::Reject:
		return "reject";
	case GameDllUserMessageRegistrationAction::ReturnExisting:
		return "return-existing";
	case GameDllUserMessageRegistrationAction::RegisterNew:
		return "register-new";
	}

	return "unknown";
}

const char *GameDllUserMessageRejectReasonName(
	GameDllUserMessageRejectReason reason)
{
	switch (reason)
	{
	case GameDllUserMessageRejectReason::None:
		return "none";
	case GameDllUserMessageRejectReason::EmptyName:
		return "empty-name";
	case GameDllUserMessageRejectReason::NameTooLong:
		return "name-too-long";
	case GameDllUserMessageRejectReason::SizeTooLarge:
		return "size-too-large";
	case GameDllUserMessageRejectReason::CapacityExceeded:
		return "capacity-exceeded";
	}

	return "unknown";
}

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
