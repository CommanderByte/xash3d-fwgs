#ifndef XASH_ENGINE_SERVER_FILTER_ADAPTER_H
#define XASH_ENGINE_SERVER_FILTER_ADAPTER_H

struct netadr_s;
typedef struct netadr_s netadr_t;

#ifdef __cplusplus
extern "C" {
#endif

int SV_ServerFilter_RuleIsActive(double endTime, double now);
int SV_ServerFilter_IdRuleMatches(const char *candidate, const char *ruleId);
int SV_ServerFilter_IpRuleMatchesAddress(
	const netadr_t *ruleAddress,
	unsigned int rulePrefixLength,
	const netadr_t *candidate);
int SV_ServerFilter_IpRemovalSelectorMatchesRule(
	const netadr_t *selectorAddress,
	unsigned int selectorPrefixLength,
	const netadr_t *ruleAddress,
	unsigned int rulePrefixLength);

#ifdef __cplusplus
}
#endif

#endif
