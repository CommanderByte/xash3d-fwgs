#ifndef XASH_ENGINE_SERVER_GAME_DLL_PAYLOAD_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_GAME_DLL_PAYLOAD_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

enum sv_gamedll_lightstyle_action_e
{
	SV_GAMEDLL_LIGHTSTYLE_SET_STYLE = 0,
	SV_GAMEDLL_LIGHTSTYLE_SKIP_LOADGAME = 1,
	SV_GAMEDLL_LIGHTSTYLE_FATAL_STYLE_OVERFLOW = 2
};

enum sv_gamedll_make_static_action_e
{
	SV_GAMEDLL_MAKE_STATIC_IGNORE_INVALID_ENTITY = 0,
	SV_GAMEDLL_MAKE_STATIC_CAPTURE_BASELINE = 1
};

typedef struct sv_gamedll_sound_route_s
{
	int destination;
	int filter_client;
} sv_gamedll_sound_route_t;

typedef struct sv_gamedll_ambient_sound_plan_s
{
	int flags;
	int destination;
} sv_gamedll_ambient_sound_plan_t;

typedef struct sv_gamedll_particle_plan_s
{
	int should_write;
	int direction_x;
	int direction_y;
	int direction_z;
	int count;
	int color;
} sv_gamedll_particle_plan_t;

typedef struct sv_gamedll_lightstyle_plan_s
{
	int action;
	int style;
} sv_gamedll_lightstyle_plan_t;

typedef struct sv_gamedll_static_decal_plan_s
{
	int flags;
	float scale;
} sv_gamedll_static_decal_plan_t;

sv_gamedll_sound_route_t SV_GameDllPayload_BuildStartSoundRoute(
	int flags,
	int channel,
	int max_clients,
	int quake_compatible);
sv_gamedll_ambient_sound_plan_t SV_GameDllPayload_BuildAmbientSoundPlan(
	int server_loading,
	int flags);
sv_gamedll_particle_plan_t SV_GameDllPayload_BuildParticlePlan(
	int bytes_left,
	float direction_x,
	float direction_y,
	float direction_z,
	float count,
	float color);
int SV_GameDllPayload_ParticleHasWritableBuffer(int bytes_left);
sv_gamedll_lightstyle_plan_t SV_GameDllPayload_BuildLightStylePlan(
	int style,
	int max_lightstyles,
	int loadgame);
sv_gamedll_static_decal_plan_t SV_GameDllPayload_BuildStaticDecalPlan(
	int permanent_flag);
int SV_GameDllPayload_BuildMakeStaticAction(int valid_entity);

#ifdef __cplusplus
}
#endif

#endif
