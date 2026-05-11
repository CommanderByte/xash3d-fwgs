#ifndef XASH_TEST_ENGINE_PMOVE_USERCMD_FIXTURE_COMMON_HPP
#define XASH_TEST_ENGINE_PMOVE_USERCMD_FIXTURE_COMMON_HPP

#include <cstddef>

#include "engine/server/world/server_pmove_bridge_policy.hpp"

namespace xash
{
namespace test
{
namespace engine
{

constexpr int kPmoveFixtureDroppedCommandLimit = 24;
constexpr std::size_t kPmoveFixtureMaxCommands = 8;

struct PmoveFixtureVec3
{
	float x;
	float y;
	float z;
};

struct PmoveFixtureUsercmd
{
	int msec;
	int buttons;
	int impulse;
	int lightlevel;
	int lerpMsec;
	float forwardmove;
	float sidemove;
	float upmove;
	PmoveFixtureVec3 viewangles;
};

struct PmoveFixtureRunStep
{
	bool useLastCommand;
	int commandIndex;
	int randomSeed;
};

struct PmoveFixtureRunPlan
{
	PmoveFixtureRunStep steps[kPmoveFixtureMaxCommands * 2];
	std::size_t count;
};

struct PmoveFixturePlayerState
{
	int entityIndex;
	int maxClients;
	bool ducking;
	float timebase;
	float movevarsMaxspeed;
	float health;
	int deadflag;
	int movetype;
	int oldbuttons;
	int waterlevel;
	int watertype;
	float clientmaxspeed;
	float waterjumptime;
	PmoveFixtureVec3 origin;
	PmoveFixtureVec3 viewangles;
	PmoveFixtureVec3 velocity;
	PmoveFixtureVec3 basevelocity;
	PmoveFixtureVec3 viewOffset;
	PmoveFixtureVec3 movedir;
	PmoveFixtureVec3 punchangle;
};

struct PmoveFixtureSetupSnapshot
{
	int playerIndex;
	bool multiplayer;
	float timeMilliseconds;
	PmoveFixtureVec3 origin;
	PmoveFixtureVec3 angles;
	PmoveFixtureVec3 oldangles;
	PmoveFixtureVec3 velocity;
	PmoveFixtureVec3 basevelocity;
	PmoveFixtureVec3 viewOffset;
	PmoveFixtureVec3 movedir;
	PmoveFixtureVec3 punchangle;
	int useHull;
	bool dead;
	int deadflag;
	int movetype;
	int oldbuttons;
	int waterlevel;
	int watertype;
	float maxspeed;
	float clientmaxspeed;
	float waterjumptime;
	PmoveFixtureUsercmd cmd;
	bool runfuncs;
	int numPhysent;
	int numVisent;
	int numMoveent;
};

struct PmoveFixtureMoveResult
{
	PmoveFixtureVec3 origin;
	PmoveFixtureVec3 angles;
	PmoveFixtureVec3 velocity;
	PmoveFixtureVec3 basevelocity;
	PmoveFixtureVec3 viewOffset;
	PmoveFixtureVec3 movedir;
	PmoveFixtureVec3 punchangle;
	PmoveFixtureUsercmd cmd;
	float waterjumptime;
	float clientmaxspeed;
	int waterlevel;
	int watertype;
	int oldbuttons;
	int movetype;
	int deadflag;
	int effects;
	int flags;
	int onground;
	int numPhysent;
	bool runfuncs;
};

struct PmoveFixtureFinishSnapshot
{
	PmoveFixtureVec3 origin;
	PmoveFixtureVec3 angles;
	PmoveFixtureVec3 viewangles;
	PmoveFixtureVec3 velocity;
	PmoveFixtureVec3 basevelocity;
	PmoveFixtureVec3 viewOffset;
	PmoveFixtureVec3 movedir;
	PmoveFixtureVec3 punchangle;
	float waterjumptime;
	float clientmaxspeed;
	int waterlevel;
	int watertype;
	int oldbuttons;
	int movetype;
	int deadflag;
	int effects;
	int flags;
	bool onGround;
	bool runfuncs;
};

enum PmoveFixtureCallbackMock
{
	kPmoveFixtureCallbackPlayerTrace = 1u << 0,
	kPmoveFixtureCallbackTraceLine = 1u << 1,
	kPmoveFixtureCallbackTestPlayerPosition = 1u << 2,
	kPmoveFixtureCallbackPointContents = 1u << 3,
	kPmoveFixtureCallbackTraceTexture = 1u << 4,
	kPmoveFixtureCallbackPlaybackEvent = 1u << 5,
	kPmoveFixtureCallbackPlaySound = 1u << 6,
	kPmoveFixtureCallbackModelQueries = 1u << 7,
	kPmoveFixtureCallbackFileAccess = 1u << 8,
	kPmoveFixtureCallbackTouchReplay = 1u << 9,
};

struct PmoveFixtureUnlagHistory
{
	float realtime;
	float clientLatency;
	float maxUnlag;
	int lerpMsec;
	float nextMessageInterval;
	float pushSeconds;
};

inline PmoveFixtureVec3 FixturePmoveVec3(float x, float y, float z)
{
	PmoveFixtureVec3 value = {};
	value.x = x;
	value.y = y;
	value.z = z;
	return value;
}

inline bool FixturePmoveVec3Equal(
	const PmoveFixtureVec3 &left,
	const PmoveFixtureVec3 &right)
{
	return left.x == right.x && left.y == right.y && left.z == right.z;
}

inline PmoveFixtureUsercmd FixturePmoveUsercmd(
	int msec,
	int buttons,
	int impulse)
{
	PmoveFixtureUsercmd command = {};
	command.msec = msec;
	command.buttons = buttons;
	command.impulse = impulse;
	return command;
}

inline PmoveFixtureUsercmd BuildFrozenPmoveCommand(
	const PmoveFixtureUsercmd &command,
	bool playerFrozen)
{
	PmoveFixtureUsercmd frozen = command;
	frozen.msec = 0;
	frozen.forwardmove = 0.0f;
	frozen.sidemove = 0.0f;
	frozen.upmove = 0.0f;
	frozen.buttons = 0;

	if (playerFrozen)
		frozen.impulse = 0;

	return frozen;
}

inline double FixturePmoveCommandSeconds(const PmoveFixtureUsercmd &command)
{
	return static_cast<double>(command.msec) / 1000.0;
}

inline void FixturePmoveAppendRunStep(
	PmoveFixtureRunPlan &plan,
	bool useLastCommand,
	int commandIndex,
	int randomSeed)
{
	if (plan.count >= (kPmoveFixtureMaxCommands * 2))
		return;

	PmoveFixtureRunStep &step = plan.steps[plan.count++];
	step.useLastCommand = useLastCommand;
	step.commandIndex = commandIndex;
	step.randomSeed = randomSeed;
}

inline PmoveFixtureRunPlan BuildPmoveFixtureRunPlan(
	int dropped,
	int numBackup,
	int numCommands,
	int incomingSequence)
{
	PmoveFixtureRunPlan plan = {};

	if (dropped < kPmoveFixtureDroppedCommandLimit)
	{
		int remainingDropped = dropped;
		while (remainingDropped > numBackup)
		{
			FixturePmoveAppendRunStep(plan, true, -1, 0);
			--remainingDropped;
		}

		while (remainingDropped > 0)
		{
			const int commandIndex = numCommands + remainingDropped - 1;
			FixturePmoveAppendRunStep(
				plan,
				false,
				commandIndex,
				incomingSequence - commandIndex);
			--remainingDropped;
		}
	}

	for (int i = numCommands - 1; i >= 0; --i)
		FixturePmoveAppendRunStep(plan, false, i, incomingSequence - i);

	return plan;
}

inline double BuildPmoveFixtureRunCommandSeconds(
	const PmoveFixtureUsercmd &lastCommand,
	const PmoveFixtureUsercmd *commands,
	int dropped,
	int numBackup,
	int numCommands)
{
	double seconds = 0.0;

	if (dropped < kPmoveFixtureDroppedCommandLimit)
	{
		int remainingDropped = dropped;
		while (remainingDropped > numBackup)
		{
			seconds += FixturePmoveCommandSeconds(lastCommand);
			--remainingDropped;
		}

		while (remainingDropped > 0)
		{
			const int commandIndex = numCommands + remainingDropped - 1;
			seconds += FixturePmoveCommandSeconds(commands[commandIndex]);
			--remainingDropped;
		}
	}

	for (int i = numCommands - 1; i >= 0; --i)
		seconds += FixturePmoveCommandSeconds(commands[i]);

	return seconds;
}

inline double BuildPmoveFixtureTimeBase(
	double serverTime,
	double frameTime,
	const PmoveFixtureUsercmd &lastCommand,
	const PmoveFixtureUsercmd *commands,
	int dropped,
	int numBackup,
	int numCommands)
{
	return serverTime + frameTime -
		BuildPmoveFixtureRunCommandSeconds(
			lastCommand,
			commands,
			dropped,
			numBackup,
			numCommands);
}

inline PmoveFixtureSetupSnapshot BuildPmoveFixtureSetupSnapshot(
	const PmoveFixturePlayerState &player,
	const PmoveFixtureUsercmd &command)
{
	PmoveFixtureSetupSnapshot snapshot = {};
	snapshot.playerIndex = player.entityIndex - 1;
	snapshot.multiplayer = player.maxClients > 1;
	snapshot.timeMilliseconds = player.timebase * 1000.0f;
	snapshot.origin = player.origin;
	snapshot.angles = player.viewangles;
	snapshot.oldangles = player.viewangles;
	snapshot.velocity = player.velocity;
	snapshot.basevelocity = player.basevelocity;
	snapshot.viewOffset = player.viewOffset;
	snapshot.movedir = player.movedir;
	snapshot.punchangle = player.punchangle;
	snapshot.useHull = player.ducking ? 1 : 0;
	snapshot.dead = player.health <= 0.0f;
	snapshot.deadflag = player.deadflag;
	snapshot.movetype = player.movetype;
	snapshot.oldbuttons = player.oldbuttons;
	snapshot.waterlevel = player.waterlevel;
	snapshot.watertype = player.watertype;
	snapshot.maxspeed = player.movevarsMaxspeed;
	snapshot.clientmaxspeed = player.clientmaxspeed;
	snapshot.waterjumptime = player.waterjumptime;
	snapshot.cmd = command;
	snapshot.runfuncs = true;
	snapshot.numPhysent = 0;
	snapshot.numVisent = 0;
	snapshot.numMoveent = 0;
	return snapshot;
}

inline PmoveFixtureFinishSnapshot BuildPmoveFixtureFinishSnapshot(
	const PmoveFixtureMoveResult &move,
	bool fixAngle)
{
	PmoveFixtureFinishSnapshot snapshot = {};
	snapshot.origin = move.origin;
	snapshot.velocity = move.velocity;
	snapshot.basevelocity = move.basevelocity;
	snapshot.viewOffset = move.viewOffset;
	snapshot.movedir = move.movedir;
	snapshot.punchangle = move.punchangle;
	snapshot.waterjumptime = move.waterjumptime;
	snapshot.clientmaxspeed = move.clientmaxspeed;
	snapshot.waterlevel = move.waterlevel;
	snapshot.watertype = move.watertype;
	snapshot.oldbuttons = move.cmd.buttons;
	snapshot.movetype = move.movetype;
	snapshot.deadflag = move.deadflag;
	snapshot.effects = move.effects;
	snapshot.flags = move.flags;
	snapshot.onGround = move.onground >= 0 && move.onground < move.numPhysent;
	snapshot.runfuncs = false;

	if (!fixAngle)
	{
		snapshot.viewangles = move.angles;
		snapshot.angles.x = -(move.angles.x / 3.0f);
		snapshot.angles.y = move.angles.y;
		snapshot.angles.z = move.angles.z;
	}

	return snapshot;
}

inline unsigned int BuildPmoveFixtureRequiredCallbackMockMask()
{
	return kPmoveFixtureCallbackPlayerTrace |
		kPmoveFixtureCallbackTraceLine |
		kPmoveFixtureCallbackTestPlayerPosition |
		kPmoveFixtureCallbackPointContents |
		kPmoveFixtureCallbackTraceTexture |
		kPmoveFixtureCallbackPlaybackEvent |
		kPmoveFixtureCallbackPlaySound |
		kPmoveFixtureCallbackModelQueries |
		kPmoveFixtureCallbackFileAccess |
		kPmoveFixtureCallbackTouchReplay;
}

inline bool FixturePmoveCallbackMockMaskHas(
	unsigned int mask,
	PmoveFixtureCallbackMock callback)
{
	return (mask & callback) != 0u;
}

inline float BuildPmoveFixtureUnlagTargetTime(
	const PmoveFixtureUnlagHistory &history)
{
	const xash::engine::server::PmoveUnlagLatencyPlan latencyPlan =
		xash::engine::server::BuildPmoveUnlagLatencyPlan(
			history.clientLatency,
			history.maxUnlag);
	const float lerpSeconds = xash::engine::server::BuildPmoveLerpSeconds(
		history.lerpMsec,
		history.nextMessageInterval);

	return xash::engine::server::BuildPmoveUnlagTargetTime(
		history.realtime,
		latencyPlan.latency,
		lerpSeconds,
		history.pushSeconds);
}

}
}
}

#endif
