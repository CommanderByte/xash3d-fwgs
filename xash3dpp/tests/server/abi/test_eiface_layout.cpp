// xash3dpp — vendored game-DLL interface layout parity (Chunk 6 S6)
// Compares every function-pointer slot offset and every support-struct
// field offset of the vendored xash::abi eiface surface against the
// ACTUAL legacy headers (engine/eiface.h, common/cvardef.h), included
// verbatim in a sealed namespace below.  Slot ORDER is the ABI — any
// drift (missing slot, reorder, parameter-type width change that alters
// struct size) fails here before it can reach a game DLL.

#include <xash3dpp/abi/eiface.hpp>

#include "../../test_helpers.hpp"

#include <cstddef>

// ---------------------------------------------------------------------------
// Legacy side: real headers in a sealed namespace (test_edict_layout
// precedent).  System headers are pre-included at global scope so their
// guards suppress re-inclusion inside the namespace; const.h/custom.h/
// xash3d_types.h are guarded off and their few needed types supplied.
// ---------------------------------------------------------------------------

#include <cstdio>  // eiface.h includes <stdio.h>
#include <cstdint> // cvardef.h includes <stdint.h>

namespace legacy {

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef int   string_t;
typedef unsigned char byte;
typedef int   qboolean;
typedef struct edict_s edict_t;
typedef struct link_s
{
    struct link_s *prev, *next;
} link_t;
typedef struct customization_s customization_t; // custom.h guarded off

#define CONST_H
#define CUSTOM_H
#define XASH_TYPES_H
#define STATIC_CHECK_SIZEOF( type, size32, size64 )
#include <engine/progdefs.h>
#include <engine/edict.h>
#include <cvardef.h>
#include <engine/eiface.h>
#undef CONST_H
#undef CUSTOM_H
#undef XASH_TYPES_H
#undef STATIC_CHECK_SIZEOF

} // namespace legacy

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Slot lists (transcribed from engine/eiface.h; one offset comparison per
// slot on both sides)
// ---------------------------------------------------------------------------

#define ENGINEFUNCS_SLOTS( X ) \
    X( pfnPrecacheModel ) X( pfnPrecacheSound ) X( pfnSetModel ) \
    X( pfnModelIndex ) X( pfnModelFrames ) X( pfnSetSize ) \
    X( pfnChangeLevel ) X( pfnGetSpawnParms ) X( pfnSaveSpawnParms ) \
    X( pfnVecToYaw ) X( pfnVecToAngles ) X( pfnMoveToOrigin ) \
    X( pfnChangeYaw ) X( pfnChangePitch ) X( pfnFindEntityByString ) \
    X( pfnGetEntityIllum ) X( pfnFindEntityInSphere ) X( pfnFindClientInPVS ) \
    X( pfnEntitiesInPVS ) X( pfnMakeVectors ) X( pfnAngleVectors ) \
    X( pfnCreateEntity ) X( pfnRemoveEntity ) X( pfnCreateNamedEntity ) \
    X( pfnMakeStatic ) X( pfnEntIsOnFloor ) X( pfnDropToFloor ) \
    X( pfnWalkMove ) X( pfnSetOrigin ) X( pfnEmitSound ) \
    X( pfnEmitAmbientSound ) X( pfnTraceLine ) X( pfnTraceToss ) \
    X( pfnTraceMonsterHull ) X( pfnTraceHull ) X( pfnTraceModel ) \
    X( pfnTraceTexture ) X( pfnTraceSphere ) X( pfnGetAimVector ) \
    X( pfnServerCommand ) X( pfnServerExecute ) X( pfnClientCommand ) \
    X( pfnParticleEffect ) X( pfnLightStyle ) X( pfnDecalIndex ) \
    X( pfnPointContents ) X( pfnMessageBegin ) X( pfnMessageEnd ) \
    X( pfnWriteByte ) X( pfnWriteChar ) X( pfnWriteShort ) \
    X( pfnWriteLong ) X( pfnWriteAngle ) X( pfnWriteCoord ) \
    X( pfnWriteString ) X( pfnWriteEntity ) X( pfnCVarRegister ) \
    X( pfnCVarGetFloat ) X( pfnCVarGetString ) X( pfnCVarSetFloat ) \
    X( pfnCVarSetString ) X( pfnAlertMessage ) X( pfnEngineFprintf ) \
    X( pfnPvAllocEntPrivateData ) X( pfnPvEntPrivateData ) \
    X( pfnFreeEntPrivateData ) X( pfnSzFromIndex ) X( pfnAllocString ) \
    X( pfnGetVarsOfEnt ) X( pfnPEntityOfEntOffset ) \
    X( pfnEntOffsetOfPEntity ) X( pfnIndexOfEdict ) \
    X( pfnPEntityOfEntIndex ) X( pfnFindEntityByVars ) \
    X( pfnGetModelPtr ) X( pfnRegUserMsg ) X( pfnAnimationAutomove ) \
    X( pfnGetBonePosition ) X( pfnFunctionFromName ) \
    X( pfnNameForFunction ) X( pfnClientPrintf ) X( pfnServerPrint ) \
    X( pfnCmd_Args ) X( pfnCmd_Argv ) X( pfnCmd_Argc ) \
    X( pfnGetAttachment ) X( pfnCRC32_Init ) X( pfnCRC32_ProcessBuffer ) \
    X( pfnCRC32_ProcessByte ) X( pfnCRC32_Final ) X( pfnRandomLong ) \
    X( pfnRandomFloat ) X( pfnSetView ) X( pfnTime ) \
    X( pfnCrosshairAngle ) X( pfnLoadFileForMe ) X( pfnFreeFile ) \
    X( pfnEndSection ) X( pfnCompareFileTime ) X( pfnGetGameDir ) \
    X( pfnCvar_RegisterVariable ) X( pfnFadeClientVolume ) \
    X( pfnSetClientMaxspeed ) X( pfnCreateFakeClient ) \
    X( pfnRunPlayerMove ) X( pfnNumberOfEntities ) \
    X( pfnGetInfoKeyBuffer ) X( pfnInfoKeyValue ) X( pfnSetKeyValue ) \
    X( pfnSetClientKeyValue ) X( pfnIsMapValid ) X( pfnStaticDecal ) \
    X( pfnPrecacheGeneric ) X( pfnGetPlayerUserId ) X( pfnBuildSoundMsg ) \
    X( pfnIsDedicatedServer ) X( pfnCVarGetPointer ) \
    X( pfnGetPlayerWONId ) X( pfnInfo_RemoveKey ) \
    X( pfnGetPhysicsKeyValue ) X( pfnSetPhysicsKeyValue ) \
    X( pfnGetPhysicsInfoString ) X( pfnPrecacheEvent ) \
    X( pfnPlaybackEvent ) X( pfnSetFatPVS ) X( pfnSetFatPAS ) \
    X( pfnCheckVisibility ) X( pfnDeltaSetField ) X( pfnDeltaUnsetField ) \
    X( pfnDeltaAddEncoder ) X( pfnGetCurrentPlayer ) X( pfnCanSkipPlayer ) \
    X( pfnDeltaFindField ) X( pfnDeltaSetFieldByIndex ) \
    X( pfnDeltaUnsetFieldByIndex ) X( pfnSetGroupMask ) \
    X( pfnCreateInstancedBaseline ) X( pfnCvar_DirectSet ) \
    X( pfnForceUnmodified ) X( pfnGetPlayerStats ) \
    X( pfnAddServerCommand ) X( pfnVoice_GetClientListening ) \
    X( pfnVoice_SetClientListening ) X( pfnGetPlayerAuthId ) \
    X( pfnSequenceGet ) X( pfnSequencePickSentence ) X( pfnGetFileSize ) \
    X( pfnGetApproxWavePlayLen ) X( pfnIsCareerMatch ) \
    X( pfnGetLocalizedStringLength ) X( pfnRegisterTutorMessageShown ) \
    X( pfnGetTimesTutorMessageShown ) \
    X( pfnProcessTutorMessageDecayBuffer ) \
    X( pfnConstructTutorMessageDecayBuffer ) \
    X( pfnResetTutorMessageDecayData ) X( pfnQueryClientCvarValue ) \
    X( pfnQueryClientCvarValue2 ) X( pfnCheckParm ) \
    X( pfnPEntityOfEntIndexAllEntities )

#define DLLFUNCS_SLOTS( X ) \
    X( pfnGameInit ) X( pfnSpawn ) X( pfnThink ) X( pfnUse ) X( pfnTouch ) \
    X( pfnBlocked ) X( pfnKeyValue ) X( pfnSave ) X( pfnRestore ) \
    X( pfnSetAbsBox ) X( pfnSaveWriteFields ) X( pfnSaveReadFields ) \
    X( pfnSaveGlobalState ) X( pfnRestoreGlobalState ) \
    X( pfnResetGlobalState ) X( pfnClientConnect ) X( pfnClientDisconnect ) \
    X( pfnClientKill ) X( pfnClientPutInServer ) X( pfnClientCommand ) \
    X( pfnClientUserInfoChanged ) X( pfnServerActivate ) \
    X( pfnServerDeactivate ) X( pfnPlayerPreThink ) X( pfnPlayerPostThink ) \
    X( pfnStartFrame ) X( pfnParmsNewLevel ) X( pfnParmsChangeLevel ) \
    X( pfnGetGameDescription ) X( pfnPlayerCustomization ) \
    X( pfnSpectatorConnect ) X( pfnSpectatorDisconnect ) \
    X( pfnSpectatorThink ) X( pfnSys_Error ) X( pfnPM_Move ) \
    X( pfnPM_Init ) X( pfnPM_FindTextureType ) X( pfnSetupVisibility ) \
    X( pfnUpdateClientData ) X( pfnAddToFullPack ) X( pfnCreateBaseline ) \
    X( pfnRegisterEncoders ) X( pfnGetWeaponData ) X( pfnCmdStart ) \
    X( pfnCmdEnd ) X( pfnConnectionlessPacket ) X( pfnGetHullBounds ) \
    X( pfnCreateInstancedBaselines ) X( pfnInconsistentFile ) \
    X( pfnAllowLagCompensation )

#define NEWDLL_SLOTS( X ) \
    X( pfnOnFreeEntPrivateData ) X( pfnGameShutdown ) \
    X( pfnShouldCollide ) X( pfnCvarValue ) X( pfnCvarValue2 )

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_enginefuncs_layout()
{
    CHECK_EQ( sizeof( xash::abi::enginefuncs_t ),
              sizeof( legacy::enginefuncs_t ));
#define X( slot ) \
    CHECK_EQ( offsetof( xash::abi::enginefuncs_t, slot ), \
              offsetof( legacy::enginefuncs_t, slot ));
    ENGINEFUNCS_SLOTS( X )
#undef X
}

static void test_dll_functions_layout()
{
    CHECK_EQ( sizeof( xash::abi::DLL_FUNCTIONS ),
              sizeof( legacy::DLL_FUNCTIONS ));
#define X( slot ) \
    CHECK_EQ( offsetof( xash::abi::DLL_FUNCTIONS, slot ), \
              offsetof( legacy::DLL_FUNCTIONS, slot ));
    DLLFUNCS_SLOTS( X )
#undef X
}

static void test_new_dll_functions_layout()
{
    CHECK_EQ( sizeof( xash::abi::NEW_DLL_FUNCTIONS ),
              sizeof( legacy::NEW_DLL_FUNCTIONS ));
#define X( slot ) \
    CHECK_EQ( offsetof( xash::abi::NEW_DLL_FUNCTIONS, slot ), \
              offsetof( legacy::NEW_DLL_FUNCTIONS, slot ));
    NEWDLL_SLOTS( X )
#undef X
}

static void test_support_struct_layouts()
{
    // TraceResult
    CHECK_EQ( sizeof( xash::abi::TraceResult ), sizeof( legacy::TraceResult ));
#define TR_FIELDS( X ) \
    X( fAllSolid ) X( fStartSolid ) X( fInOpen ) X( fInWater ) \
    X( flFraction ) X( vecEndPos ) X( flPlaneDist ) X( vecPlaneNormal ) \
    X( pHit ) X( iHitgroup )
#define X( f ) \
    CHECK_EQ( offsetof( xash::abi::TraceResult, f ), \
              offsetof( legacy::TraceResult, f ));
    TR_FIELDS( X )
#undef X

    // cvar_t (game DLLs dereference .value/.string through this layout)
    CHECK_EQ( sizeof( xash::abi::cvar_t ), sizeof( legacy::cvar_t ));
    CHECK_EQ( offsetof( xash::abi::cvar_t, name ),
              offsetof( legacy::cvar_t, name ));
    CHECK_EQ( offsetof( xash::abi::cvar_t, string ),
              offsetof( legacy::cvar_t, string ));
    CHECK_EQ( offsetof( xash::abi::cvar_t, flags ),
              offsetof( legacy::cvar_t, flags ));
    CHECK_EQ( offsetof( xash::abi::cvar_t, value ),
              offsetof( legacy::cvar_t, value ));
    CHECK_EQ( offsetof( xash::abi::cvar_t, next ),
              offsetof( legacy::cvar_t, next ));

    // KeyValueData
    CHECK_EQ( sizeof( xash::abi::KeyValueData ),
              sizeof( legacy::KeyValueData ));
    CHECK_EQ( offsetof( xash::abi::KeyValueData, szClassName ),
              offsetof( legacy::KeyValueData, szClassName ));
    CHECK_EQ( offsetof( xash::abi::KeyValueData, szKeyName ),
              offsetof( legacy::KeyValueData, szKeyName ));
    CHECK_EQ( offsetof( xash::abi::KeyValueData, szValue ),
              offsetof( legacy::KeyValueData, szValue ));
    CHECK_EQ( offsetof( xash::abi::KeyValueData, fHandled ),
              offsetof( legacy::KeyValueData, fHandled ));

    // TYPEDESCRIPTION
    CHECK_EQ( sizeof( xash::abi::TYPEDESCRIPTION ),
              sizeof( legacy::TYPEDESCRIPTION ));
    CHECK_EQ( offsetof( xash::abi::TYPEDESCRIPTION, fieldType ),
              offsetof( legacy::TYPEDESCRIPTION, fieldType ));
    CHECK_EQ( offsetof( xash::abi::TYPEDESCRIPTION, fieldName ),
              offsetof( legacy::TYPEDESCRIPTION, fieldName ));
    CHECK_EQ( offsetof( xash::abi::TYPEDESCRIPTION, fieldOffset ),
              offsetof( legacy::TYPEDESCRIPTION, fieldOffset ));
    CHECK_EQ( offsetof( xash::abi::TYPEDESCRIPTION, fieldSize ),
              offsetof( legacy::TYPEDESCRIPTION, fieldSize ));
    CHECK_EQ( offsetof( xash::abi::TYPEDESCRIPTION, flags ),
              offsetof( legacy::TYPEDESCRIPTION, flags ));

    // LEVELLIST / ENTITYTABLE / SAVERESTOREDATA
    CHECK_EQ( sizeof( xash::abi::LEVELLIST ), sizeof( legacy::LEVELLIST ));
#define LL_FIELDS( X ) \
    X( mapName ) X( landmarkName ) X( pentLandmark ) X( vecLandmarkOrigin )
#define X( f ) \
    CHECK_EQ( offsetof( xash::abi::LEVELLIST, f ), \
              offsetof( legacy::LEVELLIST, f ));
    LL_FIELDS( X )
#undef X

    CHECK_EQ( sizeof( xash::abi::ENTITYTABLE ),
              sizeof( legacy::ENTITYTABLE ));
#define ET_FIELDS( X ) \
    X( id ) X( pent ) X( location ) X( size ) X( flags ) X( classname )
#define X( f ) \
    CHECK_EQ( offsetof( xash::abi::ENTITYTABLE, f ), \
              offsetof( legacy::ENTITYTABLE, f ));
    ET_FIELDS( X )
#undef X
    CHECK_EQ( sizeof( xash::abi::SAVERESTOREDATA ),
              sizeof( legacy::SAVERESTOREDATA ));
#define SRD_FIELDS( X ) \
    X( pBaseData ) X( pCurrentData ) X( size ) X( bufferSize ) \
    X( tokenSize ) X( tokenCount ) X( pTokens ) X( currentIndex ) \
    X( tableCount ) X( connectionCount ) X( pTable ) X( levelList ) \
    X( fUseLandmark ) X( szLandmarkName ) X( vecLandmarkOffset ) \
    X( time ) X( szCurrentMapName )
#define X( f ) \
    CHECK_EQ( offsetof( xash::abi::SAVERESTOREDATA, f ), \
              offsetof( legacy::SAVERESTOREDATA, f ));
    SRD_FIELDS( X )
#undef X
}

static void test_versions_and_enums()
{
    CHECK_EQ( xash::abi::k_interface_version, INTERFACE_VERSION );
    CHECK_EQ( xash::abi::k_new_dll_functions_version,
              NEW_DLL_FUNCTIONS_VERSION );

    CHECK_EQ( static_cast<int>( xash::abi::at_logged ),
              static_cast<int>( legacy::at_logged ));
    CHECK_EQ( static_cast<int>( xash::abi::print_chat ),
              static_cast<int>( legacy::print_chat ));
    CHECK_EQ( static_cast<int>( xash::abi::force_model_specifybounds_if_avail ),
              static_cast<int>( legacy::force_model_specifybounds_if_avail ));
    CHECK_EQ( static_cast<int>( xash::abi::FIELD_TYPECOUNT ),
              static_cast<int>( legacy::FIELD_TYPECOUNT ));

    CHECK_EQ( xash::abi::k_max_level_connections, MAX_LEVEL_CONNECTIONS );
    CHECK_EQ( xash::abi::k_fenttable_player,
              static_cast<unsigned>( FENTTABLE_PLAYER ));
    CHECK_EQ( xash::abi::k_fenttable_global,
              static_cast<unsigned>( FENTTABLE_GLOBAL ));
}

int main()
{
    RUN_TEST( test_enginefuncs_layout );
    RUN_TEST( test_dll_functions_layout );
    RUN_TEST( test_new_dll_functions_layout );
    RUN_TEST( test_support_struct_layouts );
    RUN_TEST( test_versions_and_enums );

    std::printf( "eiface_layout: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
