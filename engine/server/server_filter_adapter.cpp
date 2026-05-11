#include "server_filter_adapter.h"

#include "common.h"
#include "engine/server/runtime/server_filter.hpp"

#include <cstring>

namespace
{

using xash::engine::server::FilterAddress;
using xash::engine::server::IpFilterRule;

FilterAddress ConvertAddress(const netadr_t *address)
{
	if (!address)
		return {};

	const netadrtype_t type = NET_NetadrType(address);

	if (type == NA_IP)
		return FilterAddress::ipv4(address->ip[0], address->ip[1], address->ip[2], address->ip[3]);

	if (type == NA_IP6)
	{
		uint8_t bytes[16] = {};
		std::memcpy(&bytes[0], address->ip6_0, 2);
		std::memcpy(&bytes[2], address->ip6_1, 14);
		return FilterAddress::ipv6(bytes);
	}

	return {};
}

}

extern "C" int SV_ServerFilter_RuleIsActive(double endTime, double now)
{
	return xash::engine::server::ServerFilterRuleIsActive(endTime, now);
}

extern "C" int SV_ServerFilter_IdRuleMatches(const char *candidate, const char *ruleId)
{
	return xash::engine::server::IdFilterRule::LegacyPrefixMatch(candidate, ruleId);
}

extern "C" int SV_ServerFilter_IpRuleMatchesAddress(
	const netadr_t *ruleAddress,
	unsigned int rulePrefixLength,
	const netadr_t *candidate)
{
	const IpFilterRule rule(ConvertAddress(ruleAddress), rulePrefixLength);
	return rule.matchesAddress(ConvertAddress(candidate));
}

extern "C" int SV_ServerFilter_IpRemovalSelectorMatchesRule(
	const netadr_t *selectorAddress,
	unsigned int selectorPrefixLength,
	const netadr_t *ruleAddress,
	unsigned int rulePrefixLength)
{
	const IpFilterRule selector(ConvertAddress(selectorAddress), selectorPrefixLength);
	const IpFilterRule rule(ConvertAddress(ruleAddress), rulePrefixLength);
	return selector.removalSelectorMatchesRule(rule);
}
