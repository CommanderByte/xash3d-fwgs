#include <stdlib.h>
#include <string.h>

#include "engine/server/runtime/server_filter.hpp"

using namespace xash::engine::server;

static FilterAddress Ipv6(const unsigned char (&bytes)[16])
{
	return FilterAddress::ipv6(bytes);
}

static bool TestAddressMatching()
{
	const unsigned char localPrefix[16] =
	{
		0xfd, 0x18, 0xb9, 0xd4, 0x65, 0xcf, 0x83, 0xde,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	const unsigned char localAddress[16] =
	{
		0xfd, 0x18, 0xb9, 0xd4, 0x65, 0xcf, 0x83, 0xde,
		0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
	};
	const unsigned char remoteAddress[16] =
	{
		0xfe, 0x80, 0xb9, 0xd4, 0x65, 0xcf, 0x83, 0xde,
		0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
	};

	const IpFilterRule v4(FilterAddress::ipv4(192, 168, 0, 0), 16);
	const IpFilterRule v6(Ipv6(localPrefix), 64);

	return v4.matchesAddress(FilterAddress::ipv4(192, 168, 1, 25)) &&
		!v4.matchesAddress(FilterAddress::ipv4(192, 169, 1, 25)) &&
		v6.matchesAddress(Ipv6(localAddress)) &&
		!v6.matchesAddress(Ipv6(remoteAddress));
}

static bool TestRemovalSelectorCompatibility()
{
	const unsigned char fe80Prefix[16] =
	{
		0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	const unsigned char fe80Address[16] =
	{
		0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x96, 0xab, 0x9a, 0x49, 0x29, 0x44, 0x18, 0x08,
	};
	const unsigned char otherPrefix[16] =
	{
		0x2a, 0x00, 0x13, 0x70, 0x81, 0x90, 0xf9, 0xe8,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	const unsigned char otherAddress[16] =
	{
		0x2a, 0x00, 0x13, 0x70, 0x81, 0x90, 0xf9, 0xeb,
		0x38, 0x66, 0x61, 0x26, 0x33, 0x0c, 0xb8, 0x2b,
	};

	const IpFilterRule v4Wide(FilterAddress::ipv4(127, 0, 0, 0), 8);
	const IpFilterRule v4Exact(FilterAddress::ipv4(127, 0, 0, 1), 32);
	const IpFilterRule v4Other(FilterAddress::ipv4(192, 168, 0, 0), 16);
	const IpFilterRule v6Wide(Ipv6(fe80Prefix), 64);
	const IpFilterRule v6Exact(Ipv6(fe80Address), 128);
	const IpFilterRule v6OtherWide(Ipv6(otherPrefix), 62);
	const IpFilterRule v6OtherExact(Ipv6(otherAddress), 128);

	return v4Wide.removalSelectorMatchesRule(v4Wide) &&
		!v4Wide.removalSelectorMatchesRule(v4Exact) &&
		v4Exact.removalSelectorMatchesRule(v4Wide) &&
		!v4Wide.removalSelectorMatchesRule(v4Other) &&
		!v4Other.removalSelectorMatchesRule(v4Wide) &&
		!v4Wide.removalSelectorMatchesRule(v6Wide) &&
		v6Wide.removalSelectorMatchesRule(v6Wide) &&
		!v6Wide.removalSelectorMatchesRule(v6Exact) &&
		v6Exact.removalSelectorMatchesRule(v6Wide) &&
		!v6OtherWide.removalSelectorMatchesRule(v6Wide) &&
		!v6Wide.removalSelectorMatchesRule(v6OtherWide) &&
		v6OtherExact.removalSelectorMatchesRule(v6OtherWide);
}

static bool TestIpFilterList()
{
	IpFilterList list;

	list.add(IpFilterRule(FilterAddress::ipv4(10, 0, 0, 0), 8, 0.0));
	list.add(IpFilterRule(FilterAddress::ipv4(192, 168, 1, 0), 24, 15.0));
	list.add(IpFilterRule(FilterAddress::ipv4(172, 16, 0, 0), 12, 5.0));

	if (list.size() != 3)
		return false;

	if (!list.matchesAddress(FilterAddress::ipv4(192, 168, 1, 25), 10.0))
		return false;

	if (list.matchesAddress(FilterAddress::ipv4(172, 16, 1, 25), 10.0))
		return false;

	if (list.pruneExpired(10.0) != 1 || list.size() != 2)
		return false;

	const size_t removed = list.removeMatchingSelector(
		IpFilterRule(FilterAddress::ipv4(10, 0, 0, 5), 32),
		false);

	return removed == 1 &&
		list.size() == 1 &&
		list.permanentRules().empty();
}

static bool TestRuleActivity()
{
	return ServerFilterRuleIsActive(0.0, 100.0) &&
		ServerFilterRuleIsActive(100.0, 100.0) &&
		!ServerFilterRuleIsActive(99.0, 100.0);
}

static bool TestIdFilters()
{
	IdFilterList list;

	list.add(IdFilterRule("abcdef", 0.0));
	list.add(IdFilterRule("expired", 5.0));

	if (!IdFilterRule::LegacyPrefixMatch("abc", "abcdef"))
		return false;
	if (!IdFilterRule::LegacyPrefixMatch("abcdef-extra", "abcdef"))
		return false;
	if (!IdFilterRule::LegacyPrefixMatch("", "abcdef"))
		return false;
	if (IdFilterRule::LegacyPrefixMatch("abx", "abcdef"))
		return false;

	if (!list.matchesId("abc", 1.0))
		return false;
	if (list.matchesId("expired", 10.0))
		return false;

	if (list.pruneExpired(10.0) != 1 || list.size() != 1)
		return false;

	return list.remove("abcdef") &&
		list.empty();
}

static bool TestFormatting()
{
	return FormatIpFilterRule("127.0.0.0", 8, 0.0, IpFilterOutput::Human) ==
			"127.0.0.0/8 (permanent)" &&
		FormatIpFilterRule("127.0.0.0", 8, 12.5, IpFilterOutput::Human) ==
			"127.0.0.0/8 (12.500000 minutes)" &&
		FormatIpFilterRule("127.0.0.0", 8, 0.0, IpFilterOutput::Config) ==
			"addip 0 127.0.0.0/8\n" &&
		FormatIdFilterRule("ABC", 0.0, 100.0) == "ABC permanent" &&
		FormatIdFilterRule("ABC", 220.0, 100.0) == "ABC expries in 2.000000 minutes";
}

int main()
{
	if (!TestAddressMatching() ||
		!TestRemovalSelectorCompatibility() ||
		!TestIpFilterList() ||
		!TestRuleActivity() ||
		!TestIdFilters() ||
		!TestFormatting())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
