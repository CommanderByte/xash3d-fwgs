#include "engine/server/server_filter.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{

FilterAddress FilterAddress::ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	FilterAddress address = {};
	address.family = FilterAddressFamily::Ipv4;
	address.bytes[0] = a;
	address.bytes[1] = b;
	address.bytes[2] = c;
	address.bytes[3] = d;
	return address;
}

FilterAddress FilterAddress::ipv6(const uint8_t input[16])
{
	FilterAddress address = {};
	address.family = FilterAddressFamily::Ipv6;

	if (input)
		std::memcpy(address.bytes, input, 16);

	return address;
}

size_t FilterAddress::byteCount() const
{
	switch (family)
	{
	case FilterAddressFamily::Ipv4:
		return 4;
	case FilterAddressFamily::Ipv6:
		return 16;
	case FilterAddressFamily::None:
	default:
		return 0;
	}
}

unsigned FilterAddress::maxPrefixLength() const
{
	return static_cast<unsigned>(byteCount() * 8);
}

bool FilterAddress::validPrefixLength(unsigned prefixLength) const
{
	const unsigned maximum = maxPrefixLength();
	return maximum != 0 && prefixLength <= maximum;
}

bool FilterAddress::equals(const FilterAddress &other) const
{
	const size_t count = byteCount();

	return family == other.family &&
		count == other.byteCount() &&
		count != 0 &&
		std::memcmp(bytes, other.bytes, count) == 0;
}

bool FilterAddress::matchesPrefix(const FilterAddress &network, unsigned prefixLength) const
{
	if (family != network.family || !validPrefixLength(prefixLength))
		return false;

	if (prefixLength == 0)
		return true;

	const unsigned fullBytes = prefixLength / 8;
	const unsigned remainingBits = prefixLength % 8;

	if (fullBytes != 0 && std::memcmp(bytes, network.bytes, fullBytes) != 0)
		return false;

	if (remainingBits == 0)
		return true;

	const uint8_t mask = static_cast<uint8_t>(0xFFU << (8 - remainingBits));
	return (bytes[fullBytes] & mask) == (network.bytes[fullBytes] & mask);
}

bool ServerFilterRuleIsActive(double endTime, double now)
{
	return endTime == 0.0 || now <= endTime;
}

IpFilterRule::IpFilterRule()
	: m_address()
	, m_prefixLength(0)
	, m_endTime(0.0)
{
}

IpFilterRule::IpFilterRule(const FilterAddress &address, unsigned prefixLength, double endTime)
	: m_address(address)
	, m_prefixLength(prefixLength)
	, m_endTime(endTime)
{
}

const FilterAddress &IpFilterRule::address() const
{
	return m_address;
}

unsigned IpFilterRule::prefixLength() const
{
	return m_prefixLength;
}

double IpFilterRule::endTime() const
{
	return m_endTime;
}

bool IpFilterRule::activeAt(double now) const
{
	return ServerFilterRuleIsActive(m_endTime, now);
}

bool IpFilterRule::matchesAddress(const FilterAddress &candidate) const
{
	return candidate.matchesPrefix(m_address, m_prefixLength);
}

bool IpFilterRule::removalSelectorMatchesRule(const IpFilterRule &rule) const
{
	if (m_address.family != rule.m_address.family)
		return false;

	if (!m_address.validPrefixLength(m_prefixLength) ||
		!rule.m_address.validPrefixLength(rule.m_prefixLength))
	{
		return false;
	}

	if (m_prefixLength < rule.m_prefixLength)
		return false;

	if (m_prefixLength == rule.m_prefixLength)
		return m_address.equals(rule.m_address);

	return m_address.matchesPrefix(rule.m_address, rule.m_prefixLength);
}

void IpFilterList::add(const IpFilterRule &rule)
{
	m_rules.insert(m_rules.begin(), rule);
}

size_t IpFilterList::size() const
{
	return m_rules.size();
}

bool IpFilterList::empty() const
{
	return m_rules.empty();
}

void IpFilterList::clear()
{
	m_rules.clear();
}

size_t IpFilterList::pruneExpired(double now)
{
	const size_t before = m_rules.size();

	m_rules.erase(
		std::remove_if(
			m_rules.begin(),
			m_rules.end(),
			[now](const IpFilterRule &rule) { return !rule.activeAt(now); }),
		m_rules.end());

	return before - m_rules.size();
}

bool IpFilterList::matchesAddress(const FilterAddress &candidate, double now) const
{
	for (const IpFilterRule &rule : m_rules)
	{
		if (rule.activeAt(now) && rule.matchesAddress(candidate))
			return true;
	}

	return false;
}

size_t IpFilterList::removeMatchingSelector(const IpFilterRule &selector, bool removeAll)
{
	size_t removed = 0;

	for (auto it = m_rules.begin(); it != m_rules.end();)
	{
		if (!selector.removalSelectorMatchesRule(*it))
		{
			++it;
			continue;
		}

		it = m_rules.erase(it);
		++removed;

		if (!removeAll)
			break;
	}

	return removed;
}

std::vector<IpFilterRule> IpFilterList::permanentRules() const
{
	std::vector<IpFilterRule> permanent;

	for (const IpFilterRule &rule : m_rules)
	{
		if (rule.endTime() == 0.0)
			permanent.push_back(rule);
	}

	return permanent;
}

std::string FormatIpFilterRule(
	const char *addressText,
	unsigned prefixLength,
	double endTime,
	IpFilterOutput output)
{
	char buffer[256];
	const char *safeAddress = addressText ? addressText : "";

	if (output == IpFilterOutput::Config)
	{
		std::snprintf(buffer, sizeof(buffer), "addip 0 %s/%u\n", safeAddress, prefixLength);
		return buffer;
	}

	if (endTime != 0.0)
	{
		std::snprintf(buffer, sizeof(buffer), "%s/%u (%f minutes)", safeAddress, prefixLength, endTime);
		return buffer;
	}

	std::snprintf(buffer, sizeof(buffer), "%s/%u (permanent)", safeAddress, prefixLength);
	return buffer;
}

IdFilterRule::IdFilterRule()
	: m_id()
	, m_endTime(0.0)
{
}

IdFilterRule::IdFilterRule(const char *id, double endTime)
	: m_id(id ? id : "")
	, m_endTime(endTime)
{
}

const char *IdFilterRule::id() const
{
	return m_id.c_str();
}

double IdFilterRule::endTime() const
{
	return m_endTime;
}

bool IdFilterRule::activeAt(double now) const
{
	return ServerFilterRuleIsActive(m_endTime, now);
}

bool IdFilterRule::matchesId(const char *candidate) const
{
	return LegacyPrefixMatch(candidate, m_id.c_str());
}

bool IdFilterRule::LegacyPrefixMatch(const char *candidate, const char *ruleId)
{
	if (!candidate || !ruleId)
		return false;

	const size_t candidateLength = std::strlen(candidate);
	const size_t ruleLength = std::strlen(ruleId);
	const size_t compareLength = std::min(candidateLength, ruleLength);

	return std::strncmp(candidate, ruleId, compareLength) == 0;
}

void IdFilterList::add(const IdFilterRule &rule)
{
	m_rules.insert(m_rules.begin(), rule);
}

bool IdFilterList::remove(const char *id)
{
	for (auto it = m_rules.begin(); it != m_rules.end(); ++it)
	{
		if (std::strcmp(it->id(), id ? id : "") == 0)
		{
			m_rules.erase(it);
			return true;
		}
	}

	return false;
}

size_t IdFilterList::size() const
{
	return m_rules.size();
}

bool IdFilterList::empty() const
{
	return m_rules.empty();
}

void IdFilterList::clear()
{
	m_rules.clear();
}

size_t IdFilterList::pruneExpired(double now)
{
	const size_t before = m_rules.size();

	m_rules.erase(
		std::remove_if(
			m_rules.begin(),
			m_rules.end(),
			[now](const IdFilterRule &rule) { return !rule.activeAt(now); }),
		m_rules.end());

	return before - m_rules.size();
}

bool IdFilterList::matchesId(const char *candidate, double now) const
{
	for (const IdFilterRule &rule : m_rules)
	{
		if (rule.activeAt(now) && rule.matchesId(candidate))
			return true;
	}

	return false;
}

std::vector<IdFilterRule> IdFilterList::permanentRules() const
{
	std::vector<IdFilterRule> permanent;

	for (const IdFilterRule &rule : m_rules)
	{
		if (rule.endTime() == 0.0)
			permanent.push_back(rule);
	}

	return permanent;
}

std::string FormatIdFilterRule(const char *id, double endTime, double now)
{
	char buffer[256];
	const char *safeId = id ? id : "";

	if (endTime != 0.0)
	{
		std::snprintf(buffer, sizeof(buffer), "%s expries in %f minutes", safeId, (endTime - now) / 60.0);
		return buffer;
	}

	std::snprintf(buffer, sizeof(buffer), "%s permanent", safeId);
	return buffer;
}

}
}
}
