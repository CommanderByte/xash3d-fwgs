#include <cstdlib>
#include <cstring>

#include "engine/server/resources/server_reslist_policy.hpp"

using namespace xash::engine::server;

static bool TestUnsafeTokensAreSkipped()
{
	return !ClassifyReslistToken("", true).shouldIndex &&
		!ClassifyReslistToken(nullptr, true).shouldIndex &&
		!ClassifyReslistToken("../valve/gfx.wad", true).shouldIndex &&
		!ClassifyReslistToken("sound\\weapons\\bad.wav", true).shouldIndex &&
		!ClassifyReslistToken("scripts/autoexec.cfg", true).shouldIndex &&
		!ClassifyReslistToken("models/no_extension", true).shouldIndex;
}

static bool TestSoundPathNormalizesAndRoutesToSoundIndex()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("sound//weapons/pl_gun3.wav", true);

	return decision.shouldIndex &&
		decision.type == ResourceType::Sound &&
		decision.route == ReslistRoute::SoundIndex &&
		decision.normalizedPath == "sound/weapons/pl_gun3.wav" &&
		decision.indexPath == "weapons/pl_gun3.wav";
}

static bool TestUnsupportedSoundPathFallsBackToGeneric()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("sound/ambience/test.xyz", false);

	return decision.shouldIndex &&
		decision.type == ResourceType::Generic &&
		decision.route == ReslistRoute::GenericIndex &&
		decision.normalizedPath == "sound/ambience/test.xyz" &&
		decision.indexPath == "sound/ambience/test.xyz";
}

static bool TestCaseSensitiveSoundPrefix()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("Sound/weapons/pl_gun3.wav", true);
	const ReslistTokenDecision mixed =
		ClassifyReslistToken("sOuNd/weapons/pl_gun3.wav", true);
	const ReslistTokenDecision adjacent =
		ClassifyReslistToken("soundx/weapons/pl_gun3.wav", true);

	return decision.shouldIndex &&
		decision.type == ResourceType::Generic &&
		decision.route == ReslistRoute::GenericIndex &&
		decision.normalizedPath == "Sound/weapons/pl_gun3.wav" &&
		decision.indexPath == "Sound/weapons/pl_gun3.wav" &&
		mixed.shouldIndex &&
		mixed.route == ReslistRoute::GenericIndex &&
		mixed.indexPath == "sOuNd/weapons/pl_gun3.wav" &&
		adjacent.shouldIndex &&
		adjacent.route == ReslistRoute::GenericIndex &&
		adjacent.indexPath == "soundx/weapons/pl_gun3.wav";
}

static bool TestGenericFallback()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("sprites/hud640.spr", true);

	return decision.shouldIndex &&
		decision.type == ResourceType::Generic &&
		decision.route == ReslistRoute::GenericIndex &&
		decision.normalizedPath == "sprites/hud640.spr" &&
		decision.indexPath == "sprites/hud640.spr";
}

static bool TestProbeSeparatesSoundSupportCheck()
{
	const ReslistTokenProbe soundProbe =
		BuildReslistTokenProbe("sound/items/suitchargeok1.wav");
	const ReslistTokenProbe genericProbe =
		BuildReslistTokenProbe("models/w_battery.mdl");
	const ReslistTokenProbe unsafeProbe =
		BuildReslistTokenProbe("sound\\items\\bad.wav");

	return soundProbe.safe &&
		soundProbe.soundPathCandidate &&
		soundProbe.normalizedPath == "sound/items/suitchargeok1.wav" &&
		genericProbe.safe &&
		!genericProbe.soundPathCandidate &&
		genericProbe.normalizedPath == "models/w_battery.mdl" &&
		!unsafeProbe.safe;
}

static bool TestLegacySoundPathOddities()
{
	const ReslistTokenDecision dotted =
		ClassifyReslistToken("sound/.wav", true);
	const ReslistTokenDecision relative =
		ClassifyReslistToken("sound/./foo.wav", true);

	return !ClassifyReslistToken("sound/", true).shouldIndex &&
		!ClassifyReslistToken("sound/../foo.wav", true).shouldIndex &&
		dotted.shouldIndex &&
		dotted.type == ResourceType::Sound &&
		dotted.route == ReslistRoute::SoundIndex &&
		dotted.indexPath == ".wav" &&
		relative.shouldIndex &&
		relative.type == ResourceType::Sound &&
		relative.route == ReslistRoute::SoundIndex &&
		relative.indexPath == "./foo.wav";
}

int main()
{
	if (!TestUnsafeTokensAreSkipped() ||
		!TestSoundPathNormalizesAndRoutesToSoundIndex() ||
		!TestUnsupportedSoundPathFallsBackToGeneric() ||
		!TestCaseSensitiveSoundPrefix() ||
		!TestGenericFallback() ||
		!TestProbeSeparatesSoundSupportCheck() ||
		!TestLegacySoundPathOddities())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
