#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_entity_parse.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestSkipKeyValues()
{
	return BuildGameDllEntityKeyValuePlan("", "x", false, false).action ==
			GameDllEntityKeyValueAction::Skip &&
		BuildGameDllEntityKeyValuePlan("key", "", false, false).action ==
			GameDllEntityKeyValueAction::Skip &&
		BuildGameDllEntityKeyValuePlan("wad", "halflife.wad", false, false)
			.action == GameDllEntityKeyValueAction::Skip &&
		BuildGameDllEntityKeyValuePlan("_comment", "x", true, false).action ==
			GameDllEntityKeyValueAction::Skip &&
		BuildGameDllEntityKeyValuePlan("_comment", "x", false, false)
			.action == GameDllEntityKeyValueAction::StoreDeferred;
}

static bool TestClassnameOrdering()
{
	const GameDllEntityKeyValuePlan first =
		BuildGameDllEntityKeyValuePlan(
			"classname", "worldspawn", false, false);
	const GameDllEntityKeyValuePlan duplicate =
		BuildGameDllEntityKeyValuePlan(
			"classname", "monster_alien", false, true);

	return first.action == GameDllEntityKeyValueAction::HandleClassname &&
		first.keyName == "classname" &&
		first.value == "worldspawn" &&
		duplicate.action ==
			GameDllEntityKeyValueAction::SkipDuplicateClassname;
}

static bool TestDeferredKeyTrimming()
{
	const GameDllEntityKeyValuePlan key =
		BuildGameDllEntityKeyValuePlan("targetname   ", "t1", false, false);
	const GameDllEntityKeyValuePlan wadWithSpace =
		BuildGameDllEntityKeyValuePlan("wad ", "still-deferred", false, false);

	return key.action == GameDllEntityKeyValueAction::StoreDeferred &&
		key.keyName == "targetname" &&
		key.trailingSpacesTrimmed &&
		wadWithSpace.action == GameDllEntityKeyValueAction::StoreDeferred &&
		wadWithSpace.keyName == "wad" &&
		wadWithSpace.value == "still-deferred";
}

static bool TestAngleRewrite()
{
	const GameDllAngleRewritePlan positive =
		BuildGameDllAngleRewritePlan("angle", "90", 10.0f, 20.0f);
	const GameDllAngleRewritePlan up =
		BuildGameDllAngleRewritePlan("angle", "-1", 0.0f, 0.0f);
	const GameDllAngleRewritePlan down =
		BuildGameDllAngleRewritePlan("angle", "-2", 0.0f, 0.0f);
	const GameDllAngleRewritePlan invalid =
		BuildGameDllAngleRewritePlan("angle", "-3", 0.0f, 0.0f);
	const GameDllAngleRewritePlan untouched =
		BuildGameDllAngleRewritePlan("angles", "1 2 3", 0.0f, 0.0f);

	return positive.rewritten &&
		positive.keyName == "angles" &&
		positive.value == "10 90 20" &&
		up.value == "-90 0 0" &&
		down.value == "90 0 0" &&
		invalid.value == "0 0 0" &&
		!untouched.rewritten &&
		untouched.keyName == "angles" &&
		untouched.value == "1 2 3";
}

static bool TestCustomEntityKeyValue()
{
	const GameDllCustomEntityKeyValuePlan normal =
		BuildGameDllCustomEntityKeyValuePlan(false, "monster_custom");
	const GameDllCustomEntityKeyValuePlan custom =
		BuildGameDllCustomEntityKeyValuePlan(true, "monster_custom");

	return !normal.shouldSend &&
		custom.shouldSend &&
		custom.className == "custom" &&
		custom.keyName == "customclass" &&
		custom.value == "monster_custom" &&
		!custom.requireHandled;
}

static bool TestParsePlans()
{
	return BuildGameDllEntityParsePlan(false, true, false).action ==
			GameDllEntityParseAction::RejectMissingClassname &&
		BuildGameDllEntityParsePlan(true, false, false).action ==
			GameDllEntityParseAction::RejectInvalidEntity &&
		BuildGameDllEntityParsePlan(true, true, true).action ==
			GameDllEntityParseAction::RejectInvalidEntity &&
		BuildGameDllEntityParsePlan(true, true, false).action ==
			GameDllEntityParseAction::ContinueKeyValues &&
		BuildGameDllEntityParsePlan(false, true, false)
			.shouldFreeDeferredKeyValues &&
		!BuildGameDllEntityParsePlan(true, true, false)
			.shouldFreeDeferredKeyValues;
}

static bool TestLoadAndSpawnPlans()
{
	const GameDllEntitySpawnPlan accepted =
		BuildGameDllEntitySpawnPlan(0, false);
	const GameDllEntitySpawnPlan rejected =
		BuildGameDllEntitySpawnPlan(-1, false);
	const GameDllEntitySpawnPlan killme =
		BuildGameDllEntitySpawnPlan(-1, true);

	return BuildGameDllEntityLoadAction(false, false) ==
			GameDllEntityLoadAction::ParseTextEntities &&
		BuildGameDllEntityLoadAction(true, false) ==
			GameDllEntityLoadAction::ParseTextEntities &&
		BuildGameDllEntityLoadAction(true, true) ==
			GameDllEntityLoadAction::UsePhysicsOverride &&
		accepted.action == GameDllEntitySpawnAction::SpawnAccepted &&
		!accepted.shouldFreeEdict &&
		rejected.action == GameDllEntitySpawnAction::FreeAndInhibit &&
		rejected.shouldFreeEdict &&
		rejected.shouldIncrementInhibited &&
		killme.action == GameDllEntitySpawnAction::LeaveKillmeEntity &&
		!killme.shouldFreeEdict;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllEntityKeyValueActionName(
				GameDllEntityKeyValueAction::HandleClassname),
			"handle-classname") == 0 &&
		std::strcmp(
			GameDllEntityParseActionName(
				GameDllEntityParseAction::RejectInvalidEntity),
			"reject-invalid-entity") == 0 &&
		std::strcmp(
			GameDllEntityLoadActionName(
				GameDllEntityLoadAction::UsePhysicsOverride),
			"use-physics-override") == 0 &&
		std::strcmp(
			GameDllEntitySpawnActionName(
				GameDllEntitySpawnAction::FreeAndInhibit),
			"free-and-inhibit") == 0;
}

}

int main()
{
	if (!TestSkipKeyValues() ||
		!TestClassnameOrdering() ||
		!TestDeferredKeyTrimming() ||
		!TestAngleRewrite() ||
		!TestCustomEntityKeyValue() ||
		!TestParsePlans() ||
		!TestLoadAndSpawnPlans() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
