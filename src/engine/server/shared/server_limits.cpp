#include "engine/server/shared/server_limits.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr ServerConstraintDescriptor kServerConstraints[] = {
	{
		"host skip-localhost flag",
		"SVF_SKIPLOCALHOST",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"host merge-visibility flag",
		"SVF_MERGE_VISIBILITY",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"map exists flag",
		"MAP_IS_EXIST",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"map has landmark flag",
		"MAP_HAS_LANDMARK",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"map invalid version flag",
		"MAP_INVALID_VERSION",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"server spawn frame time",
		"SV_SPAWN_TIME",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"group op and",
		"GROUP_OP_AND",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"group op nand",
		"GROUP_OP_NAND",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"pushed entity stack limit",
		"MAX_PUSHED_ENTS",
		ServerConstraintRole::AbiLayout,
		true,
		false,
	},
	{
		"portal view entity limit",
		"MAX_VIEWENTS",
		ServerConstraintRole::AbiLayout,
		true,
		false,
	},
	{
		"server localinfo string capacity",
		"MAX_LOCALINFO_STRING",
		ServerConstraintRole::AbiLayout,
		true,
		false,
	},
	{
		"extended edict leaf capacity",
		"MAX_ENT_LEAFS_32",
		ServerConstraintRole::AbiLayout,
		true,
		false,
	},
	{
		"classic edict leaf capacity",
		"MAX_ENT_LEAFS_16",
		ServerConstraintRole::AbiLayout,
		true,
		false,
	},
	{
		"client resend userinfo flag",
		"FCL_RESEND_USERINFO",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client resend movevars flag",
		"FCL_RESEND_MOVEVARS",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client skip net message flag",
		"FCL_SKIP_NET_MESSAGE",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client send net message flag",
		"FCL_SEND_NET_MESSAGE",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client predict movement flag",
		"FCL_PREDICT_MOVEMENT",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client local weapons flag",
		"FCL_LOCAL_WEAPONS",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client lag compensation flag",
		"FCL_LAG_COMPENSATION",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client fake-client flag",
		"FCL_FAKECLIENT",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client HLTV proxy flag",
		"FCL_HLTV_PROXY",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client send resources flag",
		"FCL_SEND_RESOURCES",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"client force unmodified flag",
		"FCL_FORCE_UNMODIFIED",
		ServerConstraintRole::PrivateImplementation,
		false,
		true,
	},
	{
		"challenge time window",
		"CHALLENGE_WINDOW_SECONDS",
		ServerConstraintRole::NetworkProtocol,
		false,
		true,
	},
	{
		"monster normal move type",
		"MOVE_NORMAL",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"monster strafe move type",
		"MOVE_STRAFE",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"server movement epsilon",
		"MOVE_EPSILON",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
	{
		"server clip plane limit",
		"MAX_CLIP_PLANES",
		ServerConstraintRole::Gameplay,
		false,
		true,
	},
};

}

int ServerEntityLeafCapacity(bool extendedLeafs)
{
	return extendedLeafs ? kServerMaxEntLeafs32 : kServerMaxEntLeafs16;
}

int ServerUpdateMask(int updateBackup)
{
	return updateBackup - 1;
}

const ServerConstraintDescriptor *ServerConstraintDescriptors()
{
	return kServerConstraints;
}

std::size_t ServerConstraintDescriptorCount()
{
	return sizeof(kServerConstraints) / sizeof(kServerConstraints[0]);
}

const ServerConstraintDescriptor *FindServerConstraintDescriptor(
	const char *legacyName)
{
	if (!legacyName)
		return nullptr;

	for (std::size_t i = 0; i < ServerConstraintDescriptorCount(); ++i)
	{
		if (std::strcmp(kServerConstraints[i].legacyName, legacyName) == 0)
			return &kServerConstraints[i];
	}

	return nullptr;
}

const char *ServerConstraintRoleName(ServerConstraintRole role)
{
	switch (role)
	{
	case ServerConstraintRole::AbiLayout:
		return "abi-layout";
	case ServerConstraintRole::NetworkProtocol:
		return "network-protocol";
	case ServerConstraintRole::SaveFormat:
		return "save-format";
	case ServerConstraintRole::Gameplay:
		return "gameplay";
	case ServerConstraintRole::PrivateImplementation:
		return "private-implementation";
	}

	return "unknown";
}

}
}
}
