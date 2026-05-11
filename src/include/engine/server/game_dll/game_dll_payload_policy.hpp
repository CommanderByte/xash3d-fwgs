#ifndef XASH_ENGINE_SERVER_GAME_DLL_PAYLOAD_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_PAYLOAD_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kGameDllPayloadDestinationAll = 2;
constexpr int kGameDllPayloadDestinationInit = 3;
constexpr int kGameDllPayloadDestinationPasReliable = 7;

constexpr int kGameDllPayloadChannelStatic = 6;
constexpr int kGameDllPayloadSoundStopFlag = 1 << 5;
constexpr int kGameDllPayloadSoundSpawningFlag = 1 << 8;
constexpr int kGameDllPayloadSoundFilterClientFlag = 1 << 11;

constexpr int kGameDllParticleMinimumBytesLeft = 16;
constexpr float kGameDllStaticDecalScale = 1.0f;

enum class GameDllLightStyleAction
{
	SetStyle,
	SkipLoadGame,
	FatalStyleOverflow,
};

enum class GameDllMakeStaticAction
{
	IgnoreInvalidEntity,
	CaptureBaseline,
};

struct GameDllSoundRoute
{
	int destination;
	bool filterClient;
};

struct GameDllAmbientSoundPlan
{
	int flags;
	int destination;
};

struct GameDllParticlePlan
{
	bool shouldWrite;
	int direction[3];
	int count;
	int color;
};

struct GameDllLightStylePlan
{
	GameDllLightStyleAction action;
	int style;
};

struct GameDllStaticDecalPlan
{
	int flags;
	float scale;
};

GameDllSoundRoute BuildGameDllStartSoundRoute(
	int flags,
	int channel,
	int maxClients,
	bool quakeCompatible);
GameDllAmbientSoundPlan BuildGameDllAmbientSoundPlan(
	bool serverLoading,
	int flags);
GameDllParticlePlan BuildGameDllParticlePlan(
	int bytesLeft,
	float directionX,
	float directionY,
	float directionZ,
	float count,
	float color);
bool GameDllParticleHasWritableBuffer(int bytesLeft);
GameDllLightStylePlan BuildGameDllLightStylePlan(
	int style,
	int maxLightStyles,
	bool loadGame);
GameDllStaticDecalPlan BuildGameDllStaticDecalPlan(int permanentFlag);
GameDllMakeStaticAction BuildGameDllMakeStaticAction(bool validEntity);

const char *GameDllLightStyleActionName(GameDllLightStyleAction action);
const char *GameDllMakeStaticActionName(GameDllMakeStaticAction action);

}
}
}

#endif
