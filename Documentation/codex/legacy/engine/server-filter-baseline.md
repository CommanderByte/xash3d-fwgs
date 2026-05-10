# Server Filter Baseline

Phase 53 captures the current server ID/IP filter behavior before and during
the first modern route-through.

## Legacy Owner

`engine/server/sv_filter.c` owns two filter lists:

- `cidfilter_t`: linked list of player ID bans.
- `ipfilter_t`: linked list of IP/CIDR bans.

The lists remain legacy-owned after Phase 53. Modern code only owns target-
neutral matching, expiry, list-policy, and formatting helpers.

## ID Filter Behavior

Commands:

- `banid <minutes> <#userid or unique id> [kick]`
- `listid`
- `removeid <#slotnumber or uniqueid>`
- `writeid`

Compatibility notes:

- `banid` strips `STEAM_`, `VALVE_`, and `XASH_` prefixes before matching
  spawned clients by `uuid` inside `useragent`.
- `#userid` ban support is currently disabled with a `not supported` error.
- `SV_CheckID` compares only the shorter of the query ID and filter ID. This
  means a shorter query can match a longer filter and a longer query can match
  a shorter filter.
- Empty strings match by this legacy prefix rule, although command validation
  should normally prevent empty filters from being added.
- Temporary filters expire when `host.realtime > endTime`; equality is still
  active.
- `writeid` writes only permanent filters to `banned.cfg`.

## IP Filter Behavior

Commands:

- `addip <minutes> <ipaddress[/CIDR]>`
- `listip [ipaddress[/CIDR]]`
- `removeip <ipaddress[/CIDR]> [removeAll]`
- `writeip`

Compatibility notes:

- Address parsing stays in `NET_StringToFilterAdr`.
- IPv4 accepts shorthand such as `192.168`, interpreted as a wider prefix.
- IPv6 only supports prefix-style input.
- Temporary filters expire when `host.realtime > endTime`; equality is still
  active.
- `SV_CheckIP` blocks when a candidate address matches an active stored rule by
  the stored rule prefix.
- `removeip` uses a selector rule. A selector with a shorter prefix than the
  stored rule does not remove it. A selector with the same prefix removes only
  an exact same-address rule. A more-specific selector removes a stored wider
  rule when the selector address falls inside the stored rule prefix.
- `writeip` writes only permanent filters to `listip.cfg` as `addip 0 ...`.

## Existing Embedded Tests

`sv_filter.c` already contributes `Test_RunIPFilter()` when
`XASH_ENGINE_TESTS` is enabled. It covers:

- `NET_StringToFilterAdr` IPv4 and IPv6 parse cases;
- `SV_IPFilterIncludesIPFilter` selector/removal compatibility cases.

Phase 53 keeps these tests passing while adding modern unit tests under
`tests/engine/server_filter.cpp`.

## Phase 53 Route-Through

The first route-through is intentionally small:

- ID active/expired check routes through `SV_ServerFilter_RuleIsActive`.
- ID prefix matching routes through `SV_ServerFilter_IdRuleMatches`.
- IP removal selector matching routes through
  `SV_ServerFilter_IpRemovalSelectorMatchesRule`.
- IP active matching routes through `SV_ServerFilter_IpRuleMatchesAddress`.

Still legacy-owned:

- command parsing and registration;
- linked-list allocation and removal;
- `host.realtime`;
- `svs.clients` iteration and client dropping;
- `FS_*` writes for `banned.cfg` and `listip.cfg`;
- console output.
