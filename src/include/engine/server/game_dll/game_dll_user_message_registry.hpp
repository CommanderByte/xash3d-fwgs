#ifndef XASH_ENGINE_SERVER_GAME_DLL_USER_MESSAGE_REGISTRY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_USER_MESSAGE_REGISTRY_HPP

#include <stddef.h>

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kGameDllUserMessageBadMessage = 0;
constexpr int kGameDllUserMessageFirstSlot = 1;
constexpr int kGameDllUserMessageLastServiceMessage = 59;
constexpr int kGameDllUserMessageMaxPayloadBytes = 2048;
constexpr int kGameDllUserMessageMaxMessages = 197;
constexpr int kGameDllUserMessageNameCapacity = 32;

enum class GameDllUserMessageRegistrationAction
{
	Reject,
	ReturnExisting,
	RegisterNew
};

enum class GameDllUserMessageRejectReason
{
	None,
	EmptyName,
	NameTooLong,
	SizeTooLarge,
	CapacityExceeded
};

struct GameDllUserMessageSlot
{
	const char *name;
	int number;
	int size;
};

struct GameDllUserMessageRegistrationRequest
{
	const char *name;
	int requestedSize;
	const GameDllUserMessageSlot *slots;
	int slotCount;
	int nameCapacity;
	bool serverActive;
};

struct GameDllUserMessageRegistrationPlan
{
	GameDllUserMessageRegistrationAction action;
	GameDllUserMessageRejectReason rejectReason;
	int slotIndex;
	int messageNumber;
	int storedSize;
	bool resendRegistration;
};

bool IsGameDllUserMessageNameEmpty(const char *name);
bool IsGameDllUserMessageNameTooLong(const char *name, int nameCapacity);
int ClampGameDllUserMessageSize(int size);
GameDllUserMessageRegistrationPlan BuildGameDllUserMessageRegistrationPlan(
	const GameDllUserMessageRegistrationRequest &request);
const char *GameDllUserMessageRegistrationActionName(
	GameDllUserMessageRegistrationAction action);
const char *GameDllUserMessageRejectReasonName(
	GameDllUserMessageRejectReason reason);

}
}
}

#endif
