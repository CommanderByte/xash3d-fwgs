#include <cstdlib>

#include "engine/server/server_physics_routing_policy.hpp"

using namespace xash::engine::server;

namespace
{

namespace legacy
{

constexpr int kMoveTypeNone = 0;
constexpr int kMoveTypeWalk = 3;
constexpr int kMoveTypeStep = 4;
constexpr int kMoveTypeFly = 5;
constexpr int kMoveTypeToss = 6;
constexpr int kMoveTypePush = 7;
constexpr int kMoveTypeNoclip = 8;
constexpr int kMoveTypeFlyMissile = 9;
constexpr int kMoveTypeBounce = 10;
constexpr int kMoveTypeBounceMissile = 11;
constexpr int kMoveTypeFollow = 12;
constexpr int kMoveTypePushStep = 13;
constexpr int kMoveTypeCompound = 14;

}

bool TestPhysicsHandlerRouting()
{
	return SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeNone) ==
			ServerPhysicsHandler::None &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeNoclip) ==
			ServerPhysicsHandler::Noclip &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeFollow) ==
			ServerPhysicsHandler::Follow &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeCompound) ==
			ServerPhysicsHandler::Compound &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeStep) ==
			ServerPhysicsHandler::Step &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypePushStep) ==
			ServerPhysicsHandler::Step &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeFly) ==
			ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeToss) ==
			ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeBounce) ==
			ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeFlyMissile) ==
			ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(
			legacy::kMoveTypeBounceMissile) == ServerPhysicsHandler::Toss &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypePush) ==
			ServerPhysicsHandler::Pusher &&
		SelectServerPhysicsHandlerForMoveType(legacy::kMoveTypeWalk) ==
			ServerPhysicsHandler::InvalidWalk &&
		SelectServerPhysicsHandlerForMoveType(99) ==
			ServerPhysicsHandler::Unsupported;
}

bool TestPusherCandidateFilter()
{
	return !PusherConsidersMoveType(legacy::kMoveTypeNone) &&
		!PusherConsidersMoveType(legacy::kMoveTypePush) &&
		!PusherConsidersMoveType(legacy::kMoveTypeFollow) &&
		!PusherConsidersMoveType(legacy::kMoveTypeNoclip) &&
		!PusherConsidersMoveType(legacy::kMoveTypeCompound) &&
		PusherConsidersMoveType(legacy::kMoveTypeWalk) &&
		PusherConsidersMoveType(legacy::kMoveTypeStep) &&
		PusherConsidersMoveType(legacy::kMoveTypeFly) &&
		PusherConsidersMoveType(legacy::kMoveTypeToss) &&
		PusherConsidersMoveType(legacy::kMoveTypeFlyMissile) &&
		PusherConsidersMoveType(legacy::kMoveTypeBounce) &&
		PusherConsidersMoveType(legacy::kMoveTypeBounceMissile) &&
		PusherConsidersMoveType(legacy::kMoveTypePushStep) &&
		PusherConsidersMoveType(99);
}

bool TestPreciseBlockingFilter()
{
	return PushedEntityUsesPreciseBlocking(legacy::kMoveTypeWalk) &&
		PushedEntityUsesPreciseBlocking(legacy::kMoveTypeStep) &&
		PushedEntityUsesPreciseBlocking(legacy::kMoveTypePushStep) &&
		!PushedEntityUsesPreciseBlocking(legacy::kMoveTypeNone) &&
		!PushedEntityUsesPreciseBlocking(legacy::kMoveTypePush) &&
		!PushedEntityUsesPreciseBlocking(legacy::kMoveTypeFly) &&
		!PushedEntityUsesPreciseBlocking(legacy::kMoveTypeToss) &&
		!PushedEntityUsesPreciseBlocking(99);
}

}

int main()
{
	if (!TestPhysicsHandlerRouting() ||
		!TestPusherCandidateFilter() ||
		!TestPreciseBlockingFilter())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
