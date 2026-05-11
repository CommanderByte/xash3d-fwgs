#include "engine/server/server_physics_routing_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{

namespace
{

constexpr int kLegacyMoveTypeNone = 0;
constexpr int kLegacyMoveTypeWalk = 3;
constexpr int kLegacyMoveTypeStep = 4;
constexpr int kLegacyMoveTypeFly = 5;
constexpr int kLegacyMoveTypeToss = 6;
constexpr int kLegacyMoveTypePush = 7;
constexpr int kLegacyMoveTypeNoclip = 8;
constexpr int kLegacyMoveTypeFlyMissile = 9;
constexpr int kLegacyMoveTypeBounce = 10;
constexpr int kLegacyMoveTypeBounceMissile = 11;
constexpr int kLegacyMoveTypeFollow = 12;
constexpr int kLegacyMoveTypePushStep = 13;
constexpr int kLegacyMoveTypeCompound = 14;

}

ServerPhysicsHandler SelectServerPhysicsHandlerForMoveType(int legacyMoveType)
{
	switch (legacyMoveType)
	{
	case kLegacyMoveTypeNone:
		return ServerPhysicsHandler::None;
	case kLegacyMoveTypeNoclip:
		return ServerPhysicsHandler::Noclip;
	case kLegacyMoveTypeFollow:
		return ServerPhysicsHandler::Follow;
	case kLegacyMoveTypeCompound:
		return ServerPhysicsHandler::Compound;
	case kLegacyMoveTypeStep:
	case kLegacyMoveTypePushStep:
		return ServerPhysicsHandler::Step;
	case kLegacyMoveTypeFly:
	case kLegacyMoveTypeToss:
	case kLegacyMoveTypeBounce:
	case kLegacyMoveTypeFlyMissile:
	case kLegacyMoveTypeBounceMissile:
		return ServerPhysicsHandler::Toss;
	case kLegacyMoveTypePush:
		return ServerPhysicsHandler::Pusher;
	case kLegacyMoveTypeWalk:
		return ServerPhysicsHandler::InvalidWalk;
	default:
		return ServerPhysicsHandler::Unsupported;
	}
}

bool PusherConsidersMoveType(int legacyMoveType)
{
	switch (legacyMoveType)
	{
	case kLegacyMoveTypeNone:
	case kLegacyMoveTypePush:
	case kLegacyMoveTypeFollow:
	case kLegacyMoveTypeNoclip:
	case kLegacyMoveTypeCompound:
		return false;
	default:
		return true;
	}
}

bool PushedEntityUsesPreciseBlocking(int legacyMoveType)
{
	return legacyMoveType == kLegacyMoveTypeWalk ||
		legacyMoveType == kLegacyMoveTypeStep ||
		legacyMoveType == kLegacyMoveTypePushStep;
}

}
}
}
