#ifndef XASH_ENGINE_SERVER_PHYSICS_ROUTING_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_PHYSICS_ROUTING_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SV_PHYSICS_HANDLER_UNSUPPORTED -1
#define SV_PHYSICS_HANDLER_NONE 0
#define SV_PHYSICS_HANDLER_NOCLIP 1
#define SV_PHYSICS_HANDLER_FOLLOW 2
#define SV_PHYSICS_HANDLER_COMPOUND 3
#define SV_PHYSICS_HANDLER_STEP 4
#define SV_PHYSICS_HANDLER_TOSS 5
#define SV_PHYSICS_HANDLER_PUSHER 6
#define SV_PHYSICS_HANDLER_INVALID_WALK 7

int SV_PhysicsRouting_SelectHandler(int move_type);
int SV_PhysicsRouting_PusherConsidersMoveType(int move_type);
int SV_PhysicsRouting_PushedEntityUsesPreciseBlocking(int move_type);

#ifdef __cplusplus
}
#endif

#endif
