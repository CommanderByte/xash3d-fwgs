#ifndef XASH_ENGINE_SERVER_PHYSICS_ROUTING_POLICY_HPP
#define XASH_ENGINE_SERVER_PHYSICS_ROUTING_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerPhysicsHandler
{
	Unsupported = -1,
	None = 0,
	Noclip,
	Follow,
	Compound,
	Step,
	Toss,
	Pusher,
	InvalidWalk,
};

ServerPhysicsHandler SelectServerPhysicsHandlerForMoveType(int legacyMoveType);
bool PusherConsidersMoveType(int legacyMoveType);
bool PushedEntityUsesPreciseBlocking(int legacyMoveType);

}
}
}

#endif
