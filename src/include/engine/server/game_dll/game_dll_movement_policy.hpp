#ifndef XASH_ENGINE_SERVER_GAME_DLL_MOVEMENT_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_MOVEMENT_POLICY_HPP

#include <cstdint>
#include <string>

namespace xash
{
namespace engine
{
namespace server
{

constexpr unsigned int kGameDllMoveFlagFly = 1U << 0;
constexpr unsigned int kGameDllMoveFlagSwim = 1U << 1;
constexpr unsigned int kGameDllMoveFlagOnGround = 1U << 9;

constexpr int kGameDllMoveNormal = 0;

constexpr int kGameDllWalkMoveNormal = 0;
constexpr int kGameDllWalkMoveWorldOnly = 1;
constexpr int kGameDllWalkMoveCheckOnly = 2;

enum class GameDllMoveToOriginAction
{
	SkipMissingGoal,
	SkipInvalidEntity,
	SkipImmobileEntity,
	StepTowardIdealYaw,
	FlyDirectionTowardGoal,
};

enum class GameDllAngularMoveAction
{
	SkipInvalidEntity,
	ApplyAngle,
};

enum class GameDllWalkMoveAction
{
	SkipInvalidEntity,
	SkipImmobileEntity,
	MoveStepRelink,
	MoveTestWorldOnly,
	MoveStepCheckOnly,
	FatalUnknownMode,
};

enum class GameDllSetOriginAction
{
	SkipInvalidEntity,
	CopyOriginAndRelink,
};

enum class GameDllClientMaxspeedAction
{
	SkipMissingClient,
	SetMaxspeed,
};

enum class GameDllRunPlayerMoveAction
{
	SkipMissingSpawnedClient,
	SkipRealClient,
	RunFakeClientMove,
};

struct GameDllVector3
{
	float x;
	float y;
	float z;
};

struct GameDllMoveToOriginPlan
{
	GameDllMoveToOriginAction action;
	bool usesVerticalGoal;
};

struct GameDllAngularMovePlan
{
	GameDllAngularMoveAction action;
	float nextAngle;
};

struct GameDllWalkMovePlan
{
	GameDllWalkMoveAction action;
	GameDllVector3 move;
};

struct GameDllClientMaxspeedPlan
{
	GameDllClientMaxspeedAction action;
	float maxspeed;
	std::string physInfoValue;
};

struct GameDllUserCommandSnapshot
{
	GameDllVector3 viewAngles;
	float forwardMove;
	float sideMove;
	float upMove;
	std::uint16_t buttons;
	std::uint8_t impulse;
	std::uint8_t msec;
};

struct GameDllRunPlayerMovePlan
{
	GameDllRunPlayerMoveAction action;
	double timebase;
	GameDllUserCommandSnapshot command;
};

GameDllMoveToOriginPlan BuildGameDllMoveToOriginPlan(
	bool goalPresent,
	bool validEntity,
	unsigned int flags,
	int moveType);
GameDllAngularMovePlan BuildGameDllAngularMovePlan(
	bool validEntity,
	float ideal,
	float current,
	float speed);
GameDllWalkMovePlan BuildGameDllWalkMovePlan(
	bool validEntity,
	unsigned int flags,
	float yaw,
	float distance,
	int mode);
GameDllSetOriginAction BuildGameDllSetOriginAction(bool validEntity);
GameDllClientMaxspeedPlan BuildGameDllClientMaxspeedPlan(
	bool hasClient,
	float requestedMaxspeed,
	float movevarsMaxspeed);
GameDllRunPlayerMovePlan BuildGameDllRunPlayerMovePlan(
	bool hasSpawnedClient,
	bool fakeClient,
	double serverTime,
	double frameTime,
	const GameDllVector3 &viewAngles,
	float forwardMove,
	float sideMove,
	float upMove,
	std::uint16_t buttons,
	std::uint8_t impulse,
	std::uint8_t msec);
std::string BuildGameDllFakeClientName(const char *netname);

const char *GameDllMoveToOriginActionName(GameDllMoveToOriginAction action);
const char *GameDllAngularMoveActionName(GameDllAngularMoveAction action);
const char *GameDllWalkMoveActionName(GameDllWalkMoveAction action);
const char *GameDllSetOriginActionName(GameDllSetOriginAction action);
const char *GameDllClientMaxspeedActionName(
	GameDllClientMaxspeedAction action);
const char *GameDllRunPlayerMoveActionName(GameDllRunPlayerMoveAction action);

}
}
}

#endif
