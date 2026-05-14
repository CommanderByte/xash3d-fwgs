# Server Filter Migration

Phase 53 starts the server lane by moving target-neutral ID/IP filter policy
behind the legacy `sv_filter.c` surface.

## Implemented Modern Types

Modern code lives in:

- `src/include/engine/server/server_filter.hpp`
- `src/engine/server/server_filter.cpp`

Namespace:

- `xash::engine::server`

Types and helpers:

- `FilterAddressFamily`
- `FilterAddress`
- `IpFilterRule`
- `IpFilterList`
- `IdFilterRule`
- `IdFilterList`
- `ServerFilterRuleIsActive`
- `FormatIpFilterRule`
- `FormatIdFilterRule`

The modern code does not include `server.h` and does not access `sv`, `svs`,
`svgame`, `host`, `Cmd_*`, `Cvar_*`, `FS_*`, `NET_*`, or `Con_*`.

## Legacy Adapter

The adapter lives in:

- `engine/server/server_filter_adapter.h`
- `engine/server/server_filter_adapter.cpp`

Adapter functions:

- `SV_ServerFilter_RuleIsActive`
- `SV_ServerFilter_IdRuleMatches`
- `SV_ServerFilter_IpRuleMatchesAddress`
- `SV_ServerFilter_IpRemovalSelectorMatchesRule`

The adapter converts `netadr_t` into `FilterAddress` and supplies legacy time
values. It deliberately does not own the filter lists yet.

## Preserved Behavior

Phase 53 preserves:

- `SV_CheckIP` and `SV_CheckID` public behavior;
- `addip`, `listip`, `removeip`, `writeip`, `banid`, `listid`, `removeid`, and
  `writeid` command names and registration timing;
- `banned.cfg` and `listip.cfg` output ownership;
- existing embedded `Test_RunIPFilter` coverage;
- prefix-removal quirks from `SV_IPFilterIncludesIPFilter`;
- ID shorter-prefix matching behavior;
- temporary filter expiry at `now > endTime`.

## Tests

Modern focused test:

- `tests/engine/server_filter.cpp`

Coverage:

- IPv4 and IPv6 rule matching;
- legacy removal-selector behavior;
- active versus expired rules;
- permanent-rule snapshots;
- ID legacy prefix matching;
- ID/IP list removal and expiry;
- human/config formatting helpers.

Commands run during Phase 53:

- `.\waf.bat build --targets=test_engine_server_filter`
- `.\waf.bat build --alltests`
- `run-win32\xash3d.exe -dev 2 -log +fs_path +wait +wait +quit`

The runtime smoke reached first frame in 0.512 seconds and quit by command on
May 10 2026.

## Next Opportunities

Do not immediately move command handlers. The next safe filter steps are:

1. Add tests that drive `addip`, `removeip`, and `writeip` through the legacy
   command surface.
2. Move list ownership only after command/file behavior is covered.
3. Consider a snapshot formatter for `listip`/`listid` once console output has
   better test seams.
