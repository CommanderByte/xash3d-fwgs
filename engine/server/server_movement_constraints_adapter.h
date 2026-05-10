#ifndef XASH_ENGINE_SERVER_MOVEMENT_CONSTRAINTS_ADAPTER_H
#define XASH_ENGINE_SERVER_MOVEMENT_CONSTRAINTS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int SV_MovementConstraints_IsMonsterNormalMoveType(int move_type);
int SV_MovementConstraints_IsMonsterStrafeMoveType(int move_type);

#ifdef __cplusplus
}
#endif

#endif
