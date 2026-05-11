#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "progdefs.h"
#include "eiface.h"

#include "engine/server/game_dll/game_dll_enginefuncs.hpp"

using namespace xash::engine::server;

namespace
{

enum
{
	kExpectedEnginefuncSlots = 0
#define XASH_COUNT_ENGINEFUNC(slot, field, domain, owner, readiness) + 1
	XASH_ENGINEFUNC_SLOT_TABLE(XASH_COUNT_ENGINEFUNC)
#undef XASH_COUNT_ENGINEFUNC
};

static_assert(kExpectedEnginefuncSlots == 159, "enginefuncs_t slot count changed");

static const std::size_t kEnginefuncSlotSize =
	sizeof(((enginefuncs_t *)0)->pfnPrecacheModel);

static bool CheckSlot(int slot, const char *name, std::size_t offset)
{
	const EnginefuncSlotMetadata *metadata = EnginefuncMetadata();
	const int index = slot - 1;

	return index >= 0 &&
		index < EnginefuncMetadataCount() &&
		metadata[index].slot == slot &&
		std::strcmp(metadata[index].name, name) == 0 &&
		offset == static_cast<std::size_t>(index) * kEnginefuncSlotSize;
}

static bool TestMetadataCountAndAbiSize()
{
	return EnginefuncMetadataCount() == kExpectedEnginefuncSlots &&
		sizeof(enginefuncs_t) ==
			static_cast<std::size_t>(kExpectedEnginefuncSlots) *
			kEnginefuncSlotSize;
}

static bool TestMetadataMatchesEnginefuncOrder()
{
	bool ok = true;

#define XASH_CHECK_ENGINEFUNC_SLOT(slot, field, domain, owner, readiness) \
	ok = ok && CheckSlot(slot, #field, offsetof(enginefuncs_t, field));
	XASH_ENGINEFUNC_SLOT_TABLE(XASH_CHECK_ENGINEFUNC_SLOT)
#undef XASH_CHECK_ENGINEFUNC_SLOT

	return ok;
}

static bool TestNamesAreUnique()
{
	const EnginefuncSlotMetadata *metadata = EnginefuncMetadata();

	for (int i = 0; i < EnginefuncMetadataCount(); ++i)
	{
		if (!metadata[i].name || !metadata[i].name[0])
			return false;

		for (int j = i + 1; j < EnginefuncMetadataCount(); ++j)
		{
			if (std::strcmp(metadata[i].name, metadata[j].name) == 0)
				return false;
		}
	}

	return true;
}

static bool TestImportantDomainsAndReadiness()
{
	const EnginefuncSlotMetadata *messageBegin =
		FindEnginefuncSlot("pfnMessageBegin");
	const EnginefuncSlotMetadata *userMessage =
		FindEnginefuncSlot("pfnRegUserMsg");
	const EnginefuncSlotMetadata *trace =
		FindEnginefuncSlot("pfnTraceLine");
	const EnginefuncSlotMetadata *privateData =
		FindEnginefuncSlot("pfnPvAllocEntPrivateData");
	const EnginefuncSlotMetadata *last =
		FindEnginefuncSlot("pfnPEntityOfEntIndexAllEntities");

	return messageBegin &&
		messageBegin->slot == 47 &&
		messageBegin->domain == EnginefuncDomain::MessageSession &&
		messageBegin->adapterOwner == EnginefuncAdapterOwner::MessageBuffers &&
		messageBegin->readiness == EnginefuncReadiness::StartNow &&
		userMessage &&
		userMessage->slot == 76 &&
		userMessage->domain == EnginefuncDomain::MessageSession &&
		userMessage->readiness == EnginefuncReadiness::StartNow &&
		trace &&
		trace->slot == 32 &&
		trace->domain == EnginefuncDomain::TraceVisibility &&
		trace->readiness == EnginefuncReadiness::BroadSubsystemFirst &&
		privateData &&
		privateData->slot == 64 &&
		privateData->domain == EnginefuncDomain::EntityLifecycle &&
		privateData->readiness == EnginefuncReadiness::FixtureFirst &&
		last &&
		last->slot == kExpectedEnginefuncSlots &&
		last->domain == EnginefuncDomain::EntityLifecycle;
}

static bool TestLookupAndDisplayNames()
{
	return !FindEnginefuncSlot(nullptr) &&
		!FindEnginefuncSlot("") &&
		!FindEnginefuncSlot("pfnDefinitelyNotReal") &&
		std::strcmp(
			EnginefuncDomainName(EnginefuncDomain::MessageSession),
			"message-session") == 0 &&
		std::strcmp(
			EnginefuncAdapterOwnerName(EnginefuncAdapterOwner::GameBridge),
			"game-bridge") == 0 &&
		std::strcmp(
			EnginefuncReadinessName(EnginefuncReadiness::FixtureFirst),
			"fixture-first") == 0;
}

}

int main()
{
	if (!TestMetadataCountAndAbiSize() ||
		!TestMetadataMatchesEnginefuncOrder() ||
		!TestNamesAreUnique() ||
		!TestImportantDomainsAndReadiness() ||
		!TestLookupAndDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
