#include "server_reslist_policy_adapter.h"

#include "engine/server/resources/server_reslist_policy.hpp"
#include "resource_adapter_shared.hpp"
#include "utilities/path.hpp"

extern "C" int Sound_SupportedFileFormat(const char *fileext);

namespace adapter = xash::engine::server::adapter;

namespace
{

int ToLegacyRoute(xash::engine::server::ReslistRoute route)
{
	using xash::engine::server::ReslistRoute;

	switch (route)
	{
	case ReslistRoute::SoundIndex:
		return SV_RESLIST_ROUTE_SOUND_INDEX;
	case ReslistRoute::GenericIndex:
		return SV_RESLIST_ROUTE_GENERIC_INDEX;
	case ReslistRoute::Skip:
	default:
		return SV_RESLIST_ROUTE_SKIP;
	}
}

}

extern "C" sv_reslist_decision_t SV_ReslistPolicy_ClassifyToken(const char *token)
{
	const xash::engine::server::ReslistTokenProbe probe =
		xash::engine::server::BuildReslistTokenProbe(token);
	const bool soundSupported =
		probe.soundPathCandidate &&
		Sound_SupportedFileFormat(
			xash::utilities::FileExtension(probe.normalizedPath.c_str()));
	const xash::engine::server::ReslistTokenDecision modern =
		xash::engine::server::BuildReslistTokenDecision(probe, soundSupported);

	sv_reslist_decision_t legacy = {};
	legacy.should_index = modern.shouldIndex ? 1 : 0;
	legacy.type = adapter::ToLegacyResourceType(modern.type);
	legacy.route = ToLegacyRoute(modern.route);
	adapter::CopyStringToLegacyBuffer(
		legacy.normalized_path,
		sizeof(legacy.normalized_path),
		modern.normalizedPath);
	adapter::CopyStringToLegacyBuffer(
		legacy.index_path,
		sizeof(legacy.index_path),
		modern.indexPath);
	return legacy;
}
