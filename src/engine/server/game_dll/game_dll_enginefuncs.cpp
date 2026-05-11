#include "engine/server/game_dll/game_dll_enginefuncs.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

const EnginefuncSlotMetadata kEnginefuncMetadata[] =
{
#define XASH_ENGINEFUNC_METADATA_ENTRY(slot, field, domain, owner, readiness) \
	{ slot, #field, EnginefuncDomain::domain, EnginefuncAdapterOwner::owner, \
		EnginefuncReadiness::readiness },
	XASH_ENGINEFUNC_SLOT_TABLE(XASH_ENGINEFUNC_METADATA_ENTRY)
#undef XASH_ENGINEFUNC_METADATA_ENTRY
};

}

const EnginefuncSlotMetadata *EnginefuncMetadata()
{
	return kEnginefuncMetadata;
}

int EnginefuncMetadataCount()
{
	return static_cast<int>(
		sizeof(kEnginefuncMetadata) / sizeof(kEnginefuncMetadata[0]));
}

const EnginefuncSlotMetadata *FindEnginefuncSlot(const char *name)
{
	if (!name || !name[0])
		return nullptr;

	for (int i = 0; i < EnginefuncMetadataCount(); ++i)
	{
		if (std::strcmp(kEnginefuncMetadata[i].name, name) == 0)
			return &kEnginefuncMetadata[i];
	}

	return nullptr;
}

const char *EnginefuncDomainName(EnginefuncDomain domain)
{
	switch (domain)
	{
	case EnginefuncDomain::ResourceAndPrecache:
		return "resource-and-precache";
	case EnginefuncDomain::EntityLifecycle:
		return "entity-lifecycle";
	case EnginefuncDomain::WorldMovement:
		return "world-movement";
	case EnginefuncDomain::TraceVisibility:
		return "trace-visibility";
	case EnginefuncDomain::MessageSession:
		return "message-session";
	case EnginefuncDomain::SoundAndEffects:
		return "sound-and-effects";
	case EnginefuncDomain::CommandCvarOutput:
		return "command-cvar-output";
	case EnginefuncDomain::StringPool:
		return "string-pool";
	case EnginefuncDomain::ClientInfo:
		return "client-info";
	case EnginefuncDomain::DeltaBaseline:
		return "delta-baseline";
	case EnginefuncDomain::Utility:
		return "utility";
	case EnginefuncDomain::Tutor:
		return "tutor";
	case EnginefuncDomain::PhysicsExtension:
		return "physics-extension";
	}

	return "unknown";
}

const char *EnginefuncAdapterOwnerName(EnginefuncAdapterOwner owner)
{
	switch (owner)
	{
	case EnginefuncAdapterOwner::GameBridge:
		return "game-bridge";
	case EnginefuncAdapterOwner::ResourceCatalog:
		return "resource-catalog";
	case EnginefuncAdapterOwner::EntityLifecycle:
		return "entity-lifecycle";
	case EnginefuncAdapterOwner::World:
		return "world";
	case EnginefuncAdapterOwner::Movement:
		return "movement";
	case EnginefuncAdapterOwner::MessageBuffers:
		return "message-buffers";
	case EnginefuncAdapterOwner::CommandCvar:
		return "command-cvar";
	case EnginefuncAdapterOwner::StringPool:
		return "string-pool";
	case EnginefuncAdapterOwner::ClientState:
		return "client-state";
	case EnginefuncAdapterOwner::DeltaSystem:
		return "delta-system";
	case EnginefuncAdapterOwner::Filesystem:
		return "filesystem";
	case EnginefuncAdapterOwner::Utility:
		return "utility";
	case EnginefuncAdapterOwner::Platform:
		return "platform";
	case EnginefuncAdapterOwner::Tutor:
		return "tutor";
	case EnginefuncAdapterOwner::PhysicsExtension:
		return "physics-extension";
	}

	return "unknown";
}

const char *EnginefuncReadinessName(EnginefuncReadiness readiness)
{
	switch (readiness)
	{
	case EnginefuncReadiness::StartNow:
		return "start-now";
	case EnginefuncReadiness::SoonAfter:
		return "soon-after";
	case EnginefuncReadiness::FixtureFirst:
		return "fixture-first";
	case EnginefuncReadiness::BroadSubsystemFirst:
		return "broad-subsystem-first";
	}

	return "unknown";
}

}
}
}
