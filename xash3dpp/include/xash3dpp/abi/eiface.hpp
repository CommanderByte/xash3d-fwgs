#pragma once
// xash3dpp — vendored frozen game-DLL interface: enginefuncs_t,
// DLL_FUNCTIONS, NEW_DLL_FUNCTIONS + the support types they name.
// Legacy reference: engine/eiface.h (enginefuncs_t :104-285 — 159 slots,
// DLL_FUNCTIONS :411-493 — 50 slots, NEW_DLL_FUNCTIONS :500-509 — 5
// slots), common/cvardef.h (cvar_t :85-94), engine/server/sv_game.c
// :35-40 (LINK_ENTITY_FUNC / GIVEFNPTRSTODLL export typedefs).
// Deep dive: docs/legacy-survey/deep-dive-server-game-dll-bridge.md §9.
//
// Byte-exact mirror of the tables exchanged with unmodified HL game DLLs.
// Slot ORDER is the ABI — "ONLY ADD NEW FUNCTIONS TO THE END OF THIS
// STRUCT.  INTERFACE VERSION IS FROZEN AT 138" (eiface.h:286).  Member
// names, parameter types (including the `long cb` / `unsigned long`
// LLP64-era choices), and calling conventions are kept verbatim.
// tests/server/abi/test_eiface_layout.cpp pins every slot offset against
// the actual legacy header.

#include <xash3dpp/abi/edict.hpp>

#include <cstdint>
#include <cstdio>

namespace xash::abi {

// legacy: eiface.h :22 — GetEntityAPI(2) negotiation value
inline constexpr int k_interface_version = 140;
// legacy: eiface.h :498
inline constexpr int k_new_dll_functions_version = 1;

// Win32 exports GiveFnptrsToDll as __stdcall (eiface.h DLLEXPORT :37-41);
// LINK_ENTITY exports are __cdecl (sv_game.c :35-39).  Both attributes
// are accepted-and-ignored on x64 MSVC and absent elsewhere.
#if defined( _WIN32 )
#define XASH3DPP_ABI_STDCALL __stdcall
#define XASH3DPP_ABI_CDECL   __cdecl
#else
#define XASH3DPP_ABI_STDCALL
#define XASH3DPP_ABI_CDECL
#endif

// ---------------------------------------------------------------------------
// Support enums / structs named by the tables (eiface.h :43-98, :289-346)
// ---------------------------------------------------------------------------

// legacy: eiface.h :43-51 — pfnAlertMessage severity
enum ALERT_TYPE
{
    at_notice,
    at_console,   // same as at_notice, but forces a ConPrintf, not a message box
    at_aiconsole, // same as at_console, but only shown if developer level is 2!
    at_warning,
    at_error,
    at_logged,    // server print to console (only in multiplayer games)
};

// legacy: eiface.h :54-59 — pfnClientPrintf destination
enum PRINT_TYPE
{
    print_console,
    print_center,
    print_chat,
};

// legacy: eiface.h :62-68 — pfnForceUnmodified consistency mode
enum FORCE_TYPE
{
    force_exactfile,
    force_model_samebounds,
    force_model_specifybounds,
    force_model_specifybounds_if_avail,
};

// legacy: eiface.h :71-83 — the ABI trace record handed to game DLLs
// (filled from the engine trace by SV_ConvertTrace)
struct TraceResult
{
    int      fAllSolid;      // if true, plane is not valid
    int      fStartSolid;    // if true, the initial point was in a solid area
    int      fInOpen;
    int      fInWater;
    float    flFraction;     // time completed, 1.0 = didn't hit anything
    vec3_t   vecEndPos;      // final position
    float    flPlaneDist;
    vec3_t   vecPlaneNormal; // surface normal at impact
    edict_t *pHit;           // entity the surface is on
    int      iHitgroup;      // 0 == generic, non zero is specific body part
};

// legacy: eiface.h :98
using CRC32_t = unsigned int;

// legacy: common/cvardef.h :52 — "defined by server.dll"; stamped onto
// every cvar the game registers through pfnCVarRegister so they can be
// unlinked as a group at DLL unload.
inline constexpr std::uint32_t k_fcvar_extdll = 1u << 3;

// legacy: common/cvardef.h :85-94 — game DLLs hold cvar_t* from
// pfnCVarGetPointer and read/write .value/.string directly; the layout is
// as frozen as the tables (FWGS shape: uint32_t flags — same width as
// GoldSrc's int).
struct cvar_t
{
    char         *name;
    char         *string;
    std::uint32_t flags;
    float         value;
    cvar_t       *next;
};

// legacy: eiface.h :289-295 — passed to pfnKeyValue; the engine owns the
// key/value copies and frees them after the call returns
struct KeyValueData
{
    char *szClassName; // in: entity classname
    char *szKeyName;   // in: name of key
    char *szValue;     // in: value of key
    int   fHandled;    // out: DLL sets to true if key-value pair was understood
};

// legacy: eiface.h :298-346 — save/restore exchange records (consumed by
// the Chunk 8 serializer and physFuncs restore hooks; declared here
// because DLL_FUNCTIONS slots name them)
struct LEVELLIST
{
    char     mapName[32];
    char     landmarkName[32];
    edict_t *pentLandmark;
    vec3_t   vecLandmarkOrigin;
};

struct ENTITYTABLE
{
    int      id;       // ordinal ID of this entity
    edict_t *pent;     // pointer to the in-game entity
    int      location; // offset from the base data of this entity
    int      size;     // byte size of this entity's data
    int      flags;    // bit mask of transitions this entity is in the PVS of
    string_t classname;
};

inline constexpr int k_max_level_connections = 16; // eiface.h :318

inline constexpr unsigned k_fenttable_player   = 0x80000000u;
inline constexpr unsigned k_fenttable_removed  = 0x40000000u;
inline constexpr unsigned k_fenttable_moveable = 0x20000000u;
inline constexpr unsigned k_fenttable_global   = 0x10000000u;

struct SAVERESTOREDATA
{
    char        *pBaseData;    // start of all entity save data
    char        *pCurrentData; // current buffer pointer for sequential access
    int          size;         // current data size
    int          bufferSize;   // total space for data
    int          tokenSize;    // size of the linear list of tokens
    int          tokenCount;   // number of elements in the pTokens table
    char       **pTokens;      // hash table of entity strings (sparse)
    int          currentIndex; // holds a global entity table ID
    int          tableCount;   // number of elements in the entity table
    int          connectionCount; // number of elements in the levelList[]
    ENTITYTABLE *pTable;       // array of ENTITYTABLE elements (1 per entity)
    LEVELLIST    levelList[k_max_level_connections];

    // smooth transition
    int    fUseLandmark;
    char   szLandmarkName[20];
    vec3_t vecLandmarkOffset;
    float  time;
    char   szCurrentMapName[32];
};

// legacy: eiface.h :348-370
enum FIELDTYPE
{
    FIELD_FLOAT = 0,
    FIELD_STRING,
    FIELD_ENTITY,
    FIELD_CLASSPTR,
    FIELD_EHANDLE,
    FIELD_EVARS,
    FIELD_EDICT,
    FIELD_VECTOR,
    FIELD_POSITION_VECTOR,
    FIELD_POINTER,
    FIELD_INTEGER,
    FIELD_FUNCTION,
    FIELD_BOOLEAN,
    FIELD_SHORT,
    FIELD_CHARACTER,
    FIELD_TIME,
    FIELD_MODELNAME,
    FIELD_SOUNDNAME,
    FIELD_TYPECOUNT, // MUST BE LAST
};

inline constexpr short k_ftypedesc_global        = 0x0001;
inline constexpr short k_ftypedesc_save          = 0x0002;
inline constexpr short k_ftypedesc_key           = 0x0004;
inline constexpr short k_ftypedesc_functiontable = 0x0008;

// legacy: eiface.h :392-399
struct TYPEDESCRIPTION
{
    FIELDTYPE   fieldType;
    const char *fieldName;
    int         fieldOffset;
    short       fieldSize;
    short       flags;
};

// Opaque across the boundary — only pointers cross in these tables.
struct delta_s;          // net_encode delta field list
struct entity_state_t;   // vendored fully in S9 (snapshot pipeline)
struct weapon_data_s;
struct playermove_s;     // vendored fully in S8 (pmove bridge)
struct clientdata_s;
struct usercmd_s;
struct netadr_s;
struct customization_s;  // engine/custom.h — pointer-only here
using delta_t         = delta_s;
using customization_t = customization_s;

// ---------------------------------------------------------------------------
// enginefuncs_t — 159 slots, engine → game (eiface.h :104-285)
// ---------------------------------------------------------------------------

struct enginefuncs_t
{
    int      ( *pfnPrecacheModel )( const char *s );
    int      ( *pfnPrecacheSound )( const char *s );
    void     ( *pfnSetModel )( edict_t *e, const char *m );
    int      ( *pfnModelIndex )( const char *m );
    int      ( *pfnModelFrames )( int modelIndex );
    void     ( *pfnSetSize )( edict_t *e, const float *rgflMin, const float *rgflMax );
    void     ( *pfnChangeLevel )( const char *s1, const char *s2 );
    void     ( *pfnGetSpawnParms )( edict_t *ent );
    void     ( *pfnSaveSpawnParms )( edict_t *ent );
    float    ( *pfnVecToYaw )( const float *rgflVector );
    void     ( *pfnVecToAngles )( const float *rgflVectorIn, float *rgflVectorOut );
    void     ( *pfnMoveToOrigin )( edict_t *ent, const float *pflGoal, float dist, int iMoveType );
    void     ( *pfnChangeYaw )( edict_t *ent );
    void     ( *pfnChangePitch )( edict_t *ent );
    edict_t *( *pfnFindEntityByString )( edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue );
    int      ( *pfnGetEntityIllum )( edict_t *pEnt );
    edict_t *( *pfnFindEntityInSphere )( edict_t *pEdictStartSearchAfter, const float *org, float rad );
    edict_t *( *pfnFindClientInPVS )( edict_t *pEdict );
    edict_t *( *pfnEntitiesInPVS )( edict_t *pplayer );
    void     ( *pfnMakeVectors )( const float *rgflVector );
    void     ( *pfnAngleVectors )( const float *rgflVector, float *forward, float *right, float *up );
    edict_t *( *pfnCreateEntity )( void );
    void     ( *pfnRemoveEntity )( edict_t *e );
    edict_t *( *pfnCreateNamedEntity )( int className );
    void     ( *pfnMakeStatic )( edict_t *ent );
    int      ( *pfnEntIsOnFloor )( edict_t *e );
    int      ( *pfnDropToFloor )( edict_t *e );
    int      ( *pfnWalkMove )( edict_t *ent, float yaw, float dist, int iMode );
    void     ( *pfnSetOrigin )( edict_t *e, const float *rgflOrigin );
    void     ( *pfnEmitSound )( edict_t *entity, int channel, const char *sample, /*int*/ float volume, float attenuation, int fFlags, int pitch );
    void     ( *pfnEmitAmbientSound )( edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch );
    void     ( *pfnTraceLine )( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
    void     ( *pfnTraceToss )( edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr );
    int      ( *pfnTraceMonsterHull )( edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
    void     ( *pfnTraceHull )( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr );
    void     ( *pfnTraceModel )( const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr );
    const char *( *pfnTraceTexture )( edict_t *pTextureEntity, const float *v1, const float *v2 );
    void     ( *pfnTraceSphere )( const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr );
    void     ( *pfnGetAimVector )( edict_t *ent, float speed, float *rgflReturn );
    void     ( *pfnServerCommand )( const char *str );
    void     ( *pfnServerExecute )( void );
    void     ( *pfnClientCommand )( edict_t *pEdict, char *szFmt, ... );
    void     ( *pfnParticleEffect )( const float *org, const float *dir, float color, float count );
    void     ( *pfnLightStyle )( int style, const char *val );
    int      ( *pfnDecalIndex )( const char *name );
    int      ( *pfnPointContents )( const float *rgflVector );
    void     ( *pfnMessageBegin )( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed );
    void     ( *pfnMessageEnd )( void );
    void     ( *pfnWriteByte )( int iValue );
    void     ( *pfnWriteChar )( int iValue );
    void     ( *pfnWriteShort )( int iValue );
    void     ( *pfnWriteLong )( int iValue );
    void     ( *pfnWriteAngle )( float flValue );
    void     ( *pfnWriteCoord )( float flValue );
    void     ( *pfnWriteString )( const char *sz );
    void     ( *pfnWriteEntity )( int iValue );
    void     ( *pfnCVarRegister )( cvar_t *pCvar );
    float    ( *pfnCVarGetFloat )( const char *szVarName );
    const char *( *pfnCVarGetString )( const char *szVarName );
    void     ( *pfnCVarSetFloat )( const char *szVarName, float flValue );
    void     ( *pfnCVarSetString )( const char *szVarName, const char *szValue );
    void     ( *pfnAlertMessage )( ALERT_TYPE atype, char *szFmt, ... );
    void     ( *pfnEngineFprintf )( std::FILE *pfile, char *szFmt, ... );
    void    *( *pfnPvAllocEntPrivateData )( edict_t *pEdict, long cb );
    void    *( *pfnPvEntPrivateData )( edict_t *pEdict );
    void     ( *pfnFreeEntPrivateData )( edict_t *pEdict );
    const char *( *pfnSzFromIndex )( int iString );
    int      ( *pfnAllocString )( const char *szValue );
    entvars_t *( *pfnGetVarsOfEnt )( edict_t *pEdict );
    edict_t *( *pfnPEntityOfEntOffset )( int iEntOffset );
    int      ( *pfnEntOffsetOfPEntity )( const edict_t *pEdict );
    int      ( *pfnIndexOfEdict )( const edict_t *pEdict );
    edict_t *( *pfnPEntityOfEntIndex )( int iEntIndex );
    edict_t *( *pfnFindEntityByVars )( entvars_t *pvars );
    void    *( *pfnGetModelPtr )( edict_t *pEdict );
    int      ( *pfnRegUserMsg )( const char *pszName, int iSize );
    void     ( *pfnAnimationAutomove )( const edict_t *pEdict, float flTime );
    void     ( *pfnGetBonePosition )( const edict_t *pEdict, int iBone, float *rgflOrigin, float *rgflAngles );
    unsigned long ( *pfnFunctionFromName )( const char *pName );
    const char *( *pfnNameForFunction )( unsigned long function );
    void     ( *pfnClientPrintf )( edict_t *pEdict, PRINT_TYPE ptype, const char *szMsg );
    void     ( *pfnServerPrint )( const char *szMsg );
    const char *( *pfnCmd_Args )( void );
    const char *( *pfnCmd_Argv )( int argc );
    int      ( *pfnCmd_Argc )( void );
    void     ( *pfnGetAttachment )( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );
    void     ( *pfnCRC32_Init )( CRC32_t *pulCRC );
    void     ( *pfnCRC32_ProcessBuffer )( CRC32_t *pulCRC, const void *p, int len );
    void     ( *pfnCRC32_ProcessByte )( CRC32_t *pulCRC, unsigned char ch );
    CRC32_t  ( *pfnCRC32_Final )( CRC32_t pulCRC );
    int      ( *pfnRandomLong )( int lLow, int lHigh );
    float    ( *pfnRandomFloat )( float flLow, float flHigh );
    void     ( *pfnSetView )( const edict_t *pClient, const edict_t *pViewent );
    float    ( *pfnTime )( void );
    void     ( *pfnCrosshairAngle )( const edict_t *pClient, float pitch, float yaw );
    byte    *( *pfnLoadFileForMe )( const char *filename, int *pLength );
    void     ( *pfnFreeFile )( void *buffer );
    void     ( *pfnEndSection )( const char *pszSectionName );
    int      ( *pfnCompareFileTime )( const char *filename1, const char *filename2, int *iCompare );
    void     ( *pfnGetGameDir )( char *szGetGameDir );
    void     ( *pfnCvar_RegisterVariable )( cvar_t *variable );
    void     ( *pfnFadeClientVolume )( const edict_t *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds );
    void     ( *pfnSetClientMaxspeed )( const edict_t *pEdict, float fNewMaxspeed );
    edict_t *( *pfnCreateFakeClient )( const char *netname );
    void     ( *pfnRunPlayerMove )( edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec );
    int      ( *pfnNumberOfEntities )( void );
    char    *( *pfnGetInfoKeyBuffer )( edict_t *e );
    const char *( *pfnInfoKeyValue )( const char *infobuffer, const char *key );
    void     ( *pfnSetKeyValue )( char *infobuffer, char *key, char *value );
    void     ( *pfnSetClientKeyValue )( int clientIndex, char *infobuffer, char *key, char *value );
    int      ( *pfnIsMapValid )( char *filename );
    void     ( *pfnStaticDecal )( const float *origin, int decalIndex, int entityIndex, int modelIndex );
    int      ( *pfnPrecacheGeneric )( const char *s );
    int      ( *pfnGetPlayerUserId )( edict_t *e );
    void     ( *pfnBuildSoundMsg )( edict_t *entity, int channel, const char *sample, /*int*/ float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed );
    int      ( *pfnIsDedicatedServer )( void );
    cvar_t  *( *pfnCVarGetPointer )( const char *szVarName );
    unsigned int ( *pfnGetPlayerWONId )( edict_t *e );

    // YWB 8/1/99 TFF Physics additions
    void     ( *pfnInfo_RemoveKey )( char *s, const char *key );
    const char *( *pfnGetPhysicsKeyValue )( const edict_t *pClient, const char *key );
    void     ( *pfnSetPhysicsKeyValue )( const edict_t *pClient, const char *key, const char *value );
    const char *( *pfnGetPhysicsInfoString )( const edict_t *pClient );
    unsigned short ( *pfnPrecacheEvent )( int type, const char *psz );
    void     ( *pfnPlaybackEvent )( int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );

    unsigned char *( *pfnSetFatPVS )( const float *org );
    unsigned char *( *pfnSetFatPAS )( const float *org );

    int      ( *pfnCheckVisibility )( const edict_t *entity, unsigned char *pset );

    void     ( *pfnDeltaSetField )( delta_t *pFields, const char *fieldname );
    void     ( *pfnDeltaUnsetField )( delta_t *pFields, const char *fieldname );
    void     ( *pfnDeltaAddEncoder )( char *name, void ( *conditionalencode )( delta_t *pFields, const unsigned char *from, const unsigned char *to ) );
    int      ( *pfnGetCurrentPlayer )( void );
    int      ( *pfnCanSkipPlayer )( const edict_t *player );
    int      ( *pfnDeltaFindField )( delta_t *pFields, const char *fieldname );
    void     ( *pfnDeltaSetFieldByIndex )( delta_t *pFields, int fieldNumber );
    void     ( *pfnDeltaUnsetFieldByIndex )( delta_t *pFields, int fieldNumber );
    void     ( *pfnSetGroupMask )( int mask, int op );
    int      ( *pfnCreateInstancedBaseline )( int classname, entity_state_t *baseline );
    void     ( *pfnCvar_DirectSet )( cvar_t *var, const char *value );
    void     ( *pfnForceUnmodified )( FORCE_TYPE type, float *mins, float *maxs, const char *filename );
    void     ( *pfnGetPlayerStats )( const edict_t *pClient, int *ping, int *packet_loss );
    void     ( *pfnAddServerCommand )( const char *cmd_name, void ( *function )( void ) );

    // voice: player entity indices (starting at 1)
    qboolean ( *pfnVoice_GetClientListening )( int iReceiver, int iSender );
    qboolean ( *pfnVoice_SetClientListening )( int iReceiver, int iSender, qboolean bListen );

    const char *( *pfnGetPlayerAuthId )( edict_t *e );

    void    *( *pfnSequenceGet )( const char *fileName, const char *entryName );
    void    *( *pfnSequencePickSentence )( const char *groupName, int pickMethod, int *picked );
    int      ( *pfnGetFileSize )( const char *filename );
    unsigned int ( *pfnGetApproxWavePlayLen )( const char *filepath );
    int      ( *pfnIsCareerMatch )( void );
    int      ( *pfnGetLocalizedStringLength )( const char *label );
    void     ( *pfnRegisterTutorMessageShown )( int mid );
    int      ( *pfnGetTimesTutorMessageShown )( int mid );
    void     ( *pfnProcessTutorMessageDecayBuffer )( int *buffer, int bufferLength );
    void     ( *pfnConstructTutorMessageDecayBuffer )( int *buffer, int bufferLength );
    void     ( *pfnResetTutorMessageDecayData )( void );

    void     ( *pfnQueryClientCvarValue )( const edict_t *player, const char *cvarName );
    void     ( *pfnQueryClientCvarValue2 )( const edict_t *player, const char *cvarName, int requestID );
    int      ( *pfnCheckParm )( char *parm, char **ppnext );

    // added in 8279
    edict_t *( *pfnPEntityOfEntIndexAllEntities )( int iEntIndex );
};

static_assert( sizeof( enginefuncs_t ) == 159 * sizeof( void ( * )() ),
               "enginefuncs_t slot count drifted from the frozen 159" );

// ---------------------------------------------------------------------------
// DLL_FUNCTIONS — 50 slots, game → engine (eiface.h :411-493)
// ---------------------------------------------------------------------------

struct DLL_FUNCTIONS
{
    void  ( *pfnGameInit )( void );
    int   ( *pfnSpawn )( edict_t *pent );
    void  ( *pfnThink )( edict_t *pent );
    void  ( *pfnUse )( edict_t *pentUsed, edict_t *pentOther );
    void  ( *pfnTouch )( edict_t *pentTouched, edict_t *pentOther );
    void  ( *pfnBlocked )( edict_t *pentBlocked, edict_t *pentOther );
    void  ( *pfnKeyValue )( edict_t *pentKeyvalue, KeyValueData *pkvd );
    void  ( *pfnSave )( edict_t *pent, SAVERESTOREDATA *pSaveData );
    int   ( *pfnRestore )( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
    void  ( *pfnSetAbsBox )( edict_t *pent );

    void  ( *pfnSaveWriteFields )( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );
    void  ( *pfnSaveReadFields )( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );
    void  ( *pfnSaveGlobalState )( SAVERESTOREDATA * );
    void  ( *pfnRestoreGlobalState )( SAVERESTOREDATA * );
    void  ( *pfnResetGlobalState )( void );

    qboolean ( *pfnClientConnect )( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] );
    void  ( *pfnClientDisconnect )( edict_t *pEntity );
    void  ( *pfnClientKill )( edict_t *pEntity );
    void  ( *pfnClientPutInServer )( edict_t *pEntity );
    void  ( *pfnClientCommand )( edict_t *pEntity );
    void  ( *pfnClientUserInfoChanged )( edict_t *pEntity, char *infobuffer );
    void  ( *pfnServerActivate )( edict_t *pEdictList, int edictCount, int clientMax );
    void  ( *pfnServerDeactivate )( void );
    void  ( *pfnPlayerPreThink )( edict_t *pEntity );
    void  ( *pfnPlayerPostThink )( edict_t *pEntity );

    void  ( *pfnStartFrame )( void );
    void  ( *pfnParmsNewLevel )( void );
    void  ( *pfnParmsChangeLevel )( void );

    const char *( *pfnGetGameDescription )( void );
    void  ( *pfnPlayerCustomization )( edict_t *pEntity, customization_t *pCustom );

    void  ( *pfnSpectatorConnect )( edict_t *pEntity );
    void  ( *pfnSpectatorDisconnect )( edict_t *pEntity );
    void  ( *pfnSpectatorThink )( edict_t *pEntity );

    void  ( *pfnSys_Error )( const char *error_string );

    void  ( *pfnPM_Move )( playermove_s *ppmove, qboolean server );
    void  ( *pfnPM_Init )( playermove_s *ppmove );
    char  ( *pfnPM_FindTextureType )( char *name );
    void  ( *pfnSetupVisibility )( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas );
    void  ( *pfnUpdateClientData )( const edict_t *ent, int sendweapons, clientdata_s *cd );
    int   ( *pfnAddToFullPack )( entity_state_t *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet );
    void  ( *pfnCreateBaseline )( int player, int eindex, entity_state_t *baseline, edict_t *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs );
    void  ( *pfnRegisterEncoders )( void );
    int   ( *pfnGetWeaponData )( edict_t *player, weapon_data_s *info );

    void  ( *pfnCmdStart )( const edict_t *player, const usercmd_s *cmd, unsigned int random_seed );
    void  ( *pfnCmdEnd )( const edict_t *player );

    int   ( *pfnConnectionlessPacket )( const netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );
    int   ( *pfnGetHullBounds )( int hullnumber, float *mins, float *maxs );
    void  ( *pfnCreateInstancedBaselines )( void );
    int   ( *pfnInconsistentFile )( const edict_t *player, const char *filename, char *disconnect_message );
    int   ( *pfnAllowLagCompensation )( void );
};

static_assert( sizeof( DLL_FUNCTIONS ) == 50 * sizeof( void ( * )() ),
               "DLL_FUNCTIONS slot count drifted from the frozen 50" );

// ---------------------------------------------------------------------------
// NEW_DLL_FUNCTIONS — 5 optional slots (eiface.h :500-509)
// ---------------------------------------------------------------------------

struct NEW_DLL_FUNCTIONS
{
    void  ( *pfnOnFreeEntPrivateData )( edict_t *pEnt );
    void  ( *pfnGameShutdown )( void );
    int   ( *pfnShouldCollide )( edict_t *pentTouched, edict_t *pentOther );
    void  ( *pfnCvarValue )( const edict_t *pEnt, const char *value );
    void  ( *pfnCvarValue2 )( const edict_t *pEnt, int requestID, const char *cvarName, const char *value );
};

static_assert( sizeof( NEW_DLL_FUNCTIONS ) == 5 * sizeof( void ( * )() ),
               "NEW_DLL_FUNCTIONS slot count drifted from the frozen 5" );

// ---------------------------------------------------------------------------
// Export typedefs (eiface.h :510-516, sv_game.c :35-40)
// ---------------------------------------------------------------------------

using NEW_DLL_FUNCTIONS_FN = int ( * )( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
using APIFUNCTION          = int ( * )( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
using APIFUNCTION2         = int ( * )( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

using GIVEFNPTRSTODLL  = void ( XASH3DPP_ABI_STDCALL * )( enginefuncs_t *engfuncs, globalvars_t *pGlobals );
using LINK_ENTITY_FUNC = void ( XASH3DPP_ABI_CDECL * )( entvars_t *pev );

} // namespace xash::abi
