#include "server_physics_routing_policy_adapter.h"

#include "engine/server/world/server_physics_routing_policy.hpp"

static_assert(SV_PHYSICS_HANDLER_UNSUPPORTED ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Unsupported),
	"physics unsupported handler value changed");
static_assert(SV_PHYSICS_HANDLER_NONE ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::None),
	"physics none handler value changed");
static_assert(SV_PHYSICS_HANDLER_NOCLIP ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Noclip),
	"physics noclip handler value changed");
static_assert(SV_PHYSICS_HANDLER_FOLLOW ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Follow),
	"physics follow handler value changed");
static_assert(SV_PHYSICS_HANDLER_COMPOUND ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Compound),
	"physics compound handler value changed");
static_assert(SV_PHYSICS_HANDLER_STEP ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Step),
	"physics step handler value changed");
static_assert(SV_PHYSICS_HANDLER_TOSS ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Toss),
	"physics toss handler value changed");
static_assert(SV_PHYSICS_HANDLER_PUSHER ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::Pusher),
	"physics pusher handler value changed");
static_assert(SV_PHYSICS_HANDLER_INVALID_WALK ==
	static_cast<int>(xash::engine::server::ServerPhysicsHandler::InvalidWalk),
	"physics invalid-walk handler value changed");

extern "C" int SV_PhysicsRouting_SelectHandler(int move_type)
{
	return static_cast<int>(
		xash::engine::server::SelectServerPhysicsHandlerForMoveType(
			move_type));
}

extern "C" int SV_PhysicsRouting_PusherConsidersMoveType(int move_type)
{
	return xash::engine::server::PusherConsidersMoveType(move_type) ? 1 : 0;
}

extern "C" int SV_PhysicsRouting_PushedEntityUsesPreciseBlocking(int move_type)
{
	return xash::engine::server::PushedEntityUsesPreciseBlocking(
		move_type) ? 1 : 0;
}
