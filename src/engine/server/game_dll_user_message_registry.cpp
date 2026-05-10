#include "engine/server/game_dll_user_message_registry.hpp"

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

}
}
}
