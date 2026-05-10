#ifndef XASH_ENGINE_SERVER_GAME_DLL_ENGINEFUNCS_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_ENGINEFUNCS_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class EnginefuncDomain
{
	ResourceAndPrecache,
	EntityLifecycle,
	WorldMovement,
	TraceVisibility,
	MessageSession,
	SoundAndEffects,
	CommandCvarOutput,
	StringPool,
	ClientInfo,
	DeltaBaseline,
	Utility,
	Tutor,
	PhysicsExtension
};

enum class EnginefuncAdapterOwner
{
	GameBridge,
	ResourceCatalog,
	EntityLifecycle,
	World,
	Movement,
	MessageBuffers,
	CommandCvar,
	StringPool,
	ClientState,
	DeltaSystem,
	Filesystem,
	Utility,
	Platform,
	Tutor,
	PhysicsExtension
};

enum class EnginefuncReadiness
{
	StartNow,
	SoonAfter,
	FixtureFirst,
	BroadSubsystemFirst
};

struct EnginefuncSlotMetadata
{
	int slot;
	const char *name;
	EnginefuncDomain domain;
	EnginefuncAdapterOwner adapterOwner;
	EnginefuncReadiness readiness;
};

#define XASH_ENGINEFUNC_SLOT_TABLE(X) \
	X(1, pfnPrecacheModel, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(2, pfnPrecacheSound, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(3, pfnSetModel, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(4, pfnModelIndex, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(5, pfnModelFrames, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(6, pfnSetSize, WorldMovement, World, BroadSubsystemFirst) \
	X(7, pfnChangeLevel, Utility, GameBridge, FixtureFirst) \
	X(8, pfnGetSpawnParms, ClientInfo, ClientState, SoonAfter) \
	X(9, pfnSaveSpawnParms, ClientInfo, ClientState, SoonAfter) \
	X(10, pfnVecToYaw, Utility, Utility, SoonAfter) \
	X(11, pfnVecToAngles, Utility, Utility, SoonAfter) \
	X(12, pfnMoveToOrigin, WorldMovement, Movement, BroadSubsystemFirst) \
	X(13, pfnChangeYaw, WorldMovement, Movement, BroadSubsystemFirst) \
	X(14, pfnChangePitch, WorldMovement, Movement, BroadSubsystemFirst) \
	X(15, pfnFindEntityByString, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(16, pfnGetEntityIllum, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(17, pfnFindEntityInSphere, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(18, pfnFindClientInPVS, TraceVisibility, World, BroadSubsystemFirst) \
	X(19, pfnEntitiesInPVS, TraceVisibility, World, BroadSubsystemFirst) \
	X(20, pfnMakeVectors, Utility, Utility, SoonAfter) \
	X(21, pfnAngleVectors, Utility, Utility, SoonAfter) \
	X(22, pfnCreateEntity, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(23, pfnRemoveEntity, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(24, pfnCreateNamedEntity, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(25, pfnMakeStatic, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(26, pfnEntIsOnFloor, WorldMovement, World, BroadSubsystemFirst) \
	X(27, pfnDropToFloor, WorldMovement, World, BroadSubsystemFirst) \
	X(28, pfnWalkMove, WorldMovement, Movement, BroadSubsystemFirst) \
	X(29, pfnSetOrigin, WorldMovement, World, BroadSubsystemFirst) \
	X(30, pfnEmitSound, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(31, pfnEmitAmbientSound, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(32, pfnTraceLine, TraceVisibility, World, BroadSubsystemFirst) \
	X(33, pfnTraceToss, TraceVisibility, World, BroadSubsystemFirst) \
	X(34, pfnTraceMonsterHull, TraceVisibility, World, BroadSubsystemFirst) \
	X(35, pfnTraceHull, TraceVisibility, World, BroadSubsystemFirst) \
	X(36, pfnTraceModel, TraceVisibility, World, BroadSubsystemFirst) \
	X(37, pfnTraceTexture, TraceVisibility, World, BroadSubsystemFirst) \
	X(38, pfnTraceSphere, TraceVisibility, World, BroadSubsystemFirst) \
	X(39, pfnGetAimVector, WorldMovement, Movement, BroadSubsystemFirst) \
	X(40, pfnServerCommand, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(41, pfnServerExecute, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(42, pfnClientCommand, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(43, pfnParticleEffect, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(44, pfnLightStyle, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(45, pfnDecalIndex, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(46, pfnPointContents, TraceVisibility, World, BroadSubsystemFirst) \
	X(47, pfnMessageBegin, MessageSession, MessageBuffers, StartNow) \
	X(48, pfnMessageEnd, MessageSession, MessageBuffers, StartNow) \
	X(49, pfnWriteByte, MessageSession, MessageBuffers, StartNow) \
	X(50, pfnWriteChar, MessageSession, MessageBuffers, StartNow) \
	X(51, pfnWriteShort, MessageSession, MessageBuffers, StartNow) \
	X(52, pfnWriteLong, MessageSession, MessageBuffers, StartNow) \
	X(53, pfnWriteAngle, MessageSession, MessageBuffers, StartNow) \
	X(54, pfnWriteCoord, MessageSession, MessageBuffers, StartNow) \
	X(55, pfnWriteString, MessageSession, MessageBuffers, StartNow) \
	X(56, pfnWriteEntity, MessageSession, MessageBuffers, StartNow) \
	X(57, pfnCVarRegister, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(58, pfnCVarGetFloat, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(59, pfnCVarGetString, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(60, pfnCVarSetFloat, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(61, pfnCVarSetString, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(62, pfnAlertMessage, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(63, pfnEngineFprintf, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(64, pfnPvAllocEntPrivateData, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(65, pfnPvEntPrivateData, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(66, pfnFreeEntPrivateData, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(67, pfnSzFromIndex, StringPool, StringPool, FixtureFirst) \
	X(68, pfnAllocString, StringPool, StringPool, FixtureFirst) \
	X(69, pfnGetVarsOfEnt, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(70, pfnPEntityOfEntOffset, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(71, pfnEntOffsetOfPEntity, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(72, pfnIndexOfEdict, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(73, pfnPEntityOfEntIndex, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(74, pfnFindEntityByVars, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(75, pfnGetModelPtr, ResourceAndPrecache, ResourceCatalog, FixtureFirst) \
	X(76, pfnRegUserMsg, MessageSession, MessageBuffers, StartNow) \
	X(77, pfnAnimationAutomove, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(78, pfnGetBonePosition, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(79, pfnFunctionFromName, Utility, GameBridge, SoonAfter) \
	X(80, pfnNameForFunction, Utility, GameBridge, SoonAfter) \
	X(81, pfnClientPrintf, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(82, pfnServerPrint, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(83, pfnCmd_Args, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(84, pfnCmd_Argv, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(85, pfnCmd_Argc, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(86, pfnGetAttachment, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(87, pfnCRC32_Init, Utility, Utility, SoonAfter) \
	X(88, pfnCRC32_ProcessBuffer, Utility, Utility, SoonAfter) \
	X(89, pfnCRC32_ProcessByte, Utility, Utility, SoonAfter) \
	X(90, pfnCRC32_Final, Utility, Utility, SoonAfter) \
	X(91, pfnRandomLong, Utility, Utility, SoonAfter) \
	X(92, pfnRandomFloat, Utility, Utility, SoonAfter) \
	X(93, pfnSetView, ClientInfo, ClientState, SoonAfter) \
	X(94, pfnTime, Utility, Platform, SoonAfter) \
	X(95, pfnCrosshairAngle, ClientInfo, ClientState, SoonAfter) \
	X(96, pfnLoadFileForMe, Utility, Filesystem, SoonAfter) \
	X(97, pfnFreeFile, Utility, Filesystem, SoonAfter) \
	X(98, pfnEndSection, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(99, pfnCompareFileTime, Utility, Filesystem, SoonAfter) \
	X(100, pfnGetGameDir, Utility, Filesystem, SoonAfter) \
	X(101, pfnCvar_RegisterVariable, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(102, pfnFadeClientVolume, ClientInfo, ClientState, SoonAfter) \
	X(103, pfnSetClientMaxspeed, WorldMovement, Movement, BroadSubsystemFirst) \
	X(104, pfnCreateFakeClient, ClientInfo, ClientState, SoonAfter) \
	X(105, pfnRunPlayerMove, WorldMovement, Movement, BroadSubsystemFirst) \
	X(106, pfnNumberOfEntities, EntityLifecycle, EntityLifecycle, FixtureFirst) \
	X(107, pfnGetInfoKeyBuffer, ClientInfo, ClientState, SoonAfter) \
	X(108, pfnInfoKeyValue, ClientInfo, ClientState, SoonAfter) \
	X(109, pfnSetKeyValue, ClientInfo, ClientState, SoonAfter) \
	X(110, pfnSetClientKeyValue, ClientInfo, ClientState, SoonAfter) \
	X(111, pfnIsMapValid, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(112, pfnStaticDecal, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(113, pfnPrecacheGeneric, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(114, pfnGetPlayerUserId, ClientInfo, ClientState, SoonAfter) \
	X(115, pfnBuildSoundMsg, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(116, pfnIsDedicatedServer, Utility, Platform, SoonAfter) \
	X(117, pfnCVarGetPointer, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(118, pfnGetPlayerWONId, ClientInfo, ClientState, SoonAfter) \
	X(119, pfnInfo_RemoveKey, ClientInfo, ClientState, SoonAfter) \
	X(120, pfnGetPhysicsKeyValue, PhysicsExtension, PhysicsExtension, BroadSubsystemFirst) \
	X(121, pfnSetPhysicsKeyValue, PhysicsExtension, PhysicsExtension, BroadSubsystemFirst) \
	X(122, pfnGetPhysicsInfoString, PhysicsExtension, PhysicsExtension, BroadSubsystemFirst) \
	X(123, pfnPrecacheEvent, ResourceAndPrecache, ResourceCatalog, SoonAfter) \
	X(124, pfnPlaybackEvent, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(125, pfnSetFatPVS, TraceVisibility, World, BroadSubsystemFirst) \
	X(126, pfnSetFatPAS, TraceVisibility, World, BroadSubsystemFirst) \
	X(127, pfnCheckVisibility, TraceVisibility, World, BroadSubsystemFirst) \
	X(128, pfnDeltaSetField, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(129, pfnDeltaUnsetField, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(130, pfnDeltaAddEncoder, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(131, pfnGetCurrentPlayer, ClientInfo, ClientState, SoonAfter) \
	X(132, pfnCanSkipPlayer, TraceVisibility, World, BroadSubsystemFirst) \
	X(133, pfnDeltaFindField, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(134, pfnDeltaSetFieldByIndex, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(135, pfnDeltaUnsetFieldByIndex, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(136, pfnSetGroupMask, TraceVisibility, World, BroadSubsystemFirst) \
	X(137, pfnCreateInstancedBaseline, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(138, pfnCvar_DirectSet, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(139, pfnForceUnmodified, DeltaBaseline, DeltaSystem, BroadSubsystemFirst) \
	X(140, pfnGetPlayerStats, ClientInfo, ClientState, SoonAfter) \
	X(141, pfnAddServerCommand, CommandCvarOutput, CommandCvar, SoonAfter) \
	X(142, pfnVoice_GetClientListening, ClientInfo, ClientState, SoonAfter) \
	X(143, pfnVoice_SetClientListening, ClientInfo, ClientState, SoonAfter) \
	X(144, pfnGetPlayerAuthId, ClientInfo, ClientState, SoonAfter) \
	X(145, pfnSequenceGet, Utility, Utility, SoonAfter) \
	X(146, pfnSequencePickSentence, Utility, Utility, SoonAfter) \
	X(147, pfnGetFileSize, Utility, Filesystem, SoonAfter) \
	X(148, pfnGetApproxWavePlayLen, SoundAndEffects, MessageBuffers, SoonAfter) \
	X(149, pfnIsCareerMatch, Utility, GameBridge, SoonAfter) \
	X(150, pfnGetLocalizedStringLength, Tutor, Tutor, SoonAfter) \
	X(151, pfnRegisterTutorMessageShown, Tutor, Tutor, SoonAfter) \
	X(152, pfnGetTimesTutorMessageShown, Tutor, Tutor, SoonAfter) \
	X(153, pfnProcessTutorMessageDecayBuffer, Tutor, Tutor, SoonAfter) \
	X(154, pfnConstructTutorMessageDecayBuffer, Tutor, Tutor, SoonAfter) \
	X(155, pfnResetTutorMessageDecayData, Tutor, Tutor, SoonAfter) \
	X(156, pfnQueryClientCvarValue, ClientInfo, ClientState, SoonAfter) \
	X(157, pfnQueryClientCvarValue2, ClientInfo, ClientState, SoonAfter) \
	X(158, pfnCheckParm, Utility, Utility, SoonAfter) \
	X(159, pfnPEntityOfEntIndexAllEntities, EntityLifecycle, EntityLifecycle, FixtureFirst)

const EnginefuncSlotMetadata *EnginefuncMetadata();
int EnginefuncMetadataCount();
const EnginefuncSlotMetadata *FindEnginefuncSlot(const char *name);
const char *EnginefuncDomainName(EnginefuncDomain domain);
const char *EnginefuncAdapterOwnerName(EnginefuncAdapterOwner owner);
const char *EnginefuncReadinessName(EnginefuncReadiness readiness);

}
}
}

#endif
