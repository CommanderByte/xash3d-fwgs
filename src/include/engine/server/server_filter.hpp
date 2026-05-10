#ifndef XASH_ENGINE_SERVER_SERVER_FILTER_HPP
#define XASH_ENGINE_SERVER_SERVER_FILTER_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace xash
{
namespace engine
{
namespace server
{

enum class FilterAddressFamily
{
	None,
	Ipv4,
	Ipv6,
};

struct FilterAddress
{
	FilterAddressFamily family;
	uint8_t bytes[16];

	static FilterAddress ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
	static FilterAddress ipv6(const uint8_t address[16]);

	size_t byteCount() const;
	unsigned maxPrefixLength() const;
	bool validPrefixLength(unsigned prefixLength) const;
	bool equals(const FilterAddress &other) const;
	bool matchesPrefix(const FilterAddress &network, unsigned prefixLength) const;
};

bool ServerFilterRuleIsActive(double endTime, double now);

class IpFilterRule
{
public:
	IpFilterRule();
	IpFilterRule(const FilterAddress &address, unsigned prefixLength, double endTime = 0.0);

	const FilterAddress &address() const;
	unsigned prefixLength() const;
	double endTime() const;

	bool activeAt(double now) const;
	bool matchesAddress(const FilterAddress &candidate) const;
	bool removalSelectorMatchesRule(const IpFilterRule &rule) const;

private:
	FilterAddress m_address;
	unsigned m_prefixLength;
	double m_endTime;
};

class IpFilterList
{
public:
	void add(const IpFilterRule &rule);
	size_t size() const;
	bool empty() const;
	void clear();
	size_t pruneExpired(double now);
	bool matchesAddress(const FilterAddress &candidate, double now) const;
	size_t removeMatchingSelector(const IpFilterRule &selector, bool removeAll);
	std::vector<IpFilterRule> permanentRules() const;

private:
	std::vector<IpFilterRule> m_rules;
};

enum class IpFilterOutput
{
	Human,
	Config,
};

std::string FormatIpFilterRule(
	const char *addressText,
	unsigned prefixLength,
	double endTime,
	IpFilterOutput output);

class IdFilterRule
{
public:
	IdFilterRule();
	IdFilterRule(const char *id, double endTime = 0.0);

	const char *id() const;
	double endTime() const;

	bool activeAt(double now) const;
	bool matchesId(const char *candidate) const;

	static bool LegacyPrefixMatch(const char *candidate, const char *ruleId);

private:
	std::string m_id;
	double m_endTime;
};

class IdFilterList
{
public:
	void add(const IdFilterRule &rule);
	bool remove(const char *id);
	size_t size() const;
	bool empty() const;
	void clear();
	size_t pruneExpired(double now);
	bool matchesId(const char *candidate, double now) const;
	std::vector<IdFilterRule> permanentRules() const;

private:
	std::vector<IdFilterRule> m_rules;
};

std::string FormatIdFilterRule(const char *id, double endTime, double now);

}
}
}

#endif
