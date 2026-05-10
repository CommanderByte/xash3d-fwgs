# Client Rate And Userinfo Policy Baseline

Phase: 85

Legacy owner: `engine/server/sv_client.c`

## Scope

This baseline covers the low-risk policy pieces around
`SV_ShouldUpdateUserinfo()`, `SV_CheckUpdateRate()`, `SV_CheckRate()`, and the
rate/flag extraction portion of `SV_UserinfoChanged()`.

It does not move info-string parsing, duplicate-name resolution, game DLL
callbacks, logging, cvar ownership, MD5/userinfo broadcast serialization, or
client state ownership.

## Userinfo Penalty

`SV_ShouldUpdateUserinfo()` throttles frequent userinfo updates when
`sv_userinfo_enable_penalty` is enabled.

Bypass cases:

- penalty cvar disabled;
- fake clients;
- single-player games.

Legacy behavior when penalty is active:

- a zero client penalty is initialized from `sv_userinfo_penalty_time`;
- the spam window is `userinfo_next_changetime + penalty *
  sv_userinfo_penalty_multiplier`;
- a change inside that window increments `userinfo_change_attempts`;
- a change before `userinfo_next_changetime` is rejected only when attempts are
  already greater than zero;
- when attempts reach `(int)sv_userinfo_penalty_attempts`, penalty is
  multiplied by `sv_userinfo_penalty_multiplier` and attempts reset to zero;
- `userinfo_next_changetime` is always rewritten to `host.realtime + penalty *
  sv_userinfo_penalty_multiplier` after active processing.

The first quick retry can therefore be counted but still accepted. Later quick
retries can be ignored and can also trigger a penalty increase.

## Rate And Update Interval

`SV_UserinfoChanged()` reads `rate` and `cl_updaterate` from the userinfo string.
Missing or malformed integer values behave like zero because parsing uses
`Q_atoi()`.

Rate behavior:

- `rate <= 0` becomes `DEFAULT_RATE`;
- the requested rate is hard-clamped to `MIN_RATE` / `MAX_RATE`;
- `SV_CheckRate()` currently preserves its input. Its branches compare
  `sv_maxrate` and `sv_minrate`, but return the original rate in every path.
  This is treated as compatibility behavior for Phase 85 rather than fixed.

Update interval behavior:

- `cl_updaterate <= 0` becomes 20 updates per second;
- the base interval is `1.0 / cl_updaterate`;
- `sv_maxupdaterate` imposes a minimum interval of `1.0 / sv_maxupdaterate`;
- `sv_minupdaterate` imposes a maximum interval of `1.0 / sv_minupdaterate`.

## Userinfo-Derived Flags

`SV_UserinfoChanged()` updates three client flags:

- `cl_nopred != 0` clears `FCL_PREDICT_MOVEMENT`; otherwise prediction is set;
- `cl_lc != 0` sets `FCL_LAG_COMPENSATION`; otherwise it is cleared;
- `cl_lw != 0` sets `FCL_LOCAL_WEAPONS`; otherwise it is cleared.

The game DLL callback `pfnClientUserInfoChanged()` still runs after these local
updates and may mutate the userinfo string. Legacy code then copies the final
`name` value back into `cl->name` and `ent->v.netname`.

## Phase 85 Route-Through Boundary

Phase 85 routes only target-neutral decisions through
`src/engine/server/client_policy.cpp`:

- userinfo penalty plan;
- requested rate/default/hard clamp;
- requested update-rate default;
- update interval cvar limits;
- legacy server rate cvar behavior;
- prediction, lag compensation, and local-weapons flag decisions.

Legacy code still owns:

- `Info_*` reads and writes;
- duplicate-name suffix generation;
- `Con_Reportf()` diagnostics;
- cvar reads;
- game DLL calls;
- direct mutation of `sv_client_t`, `edict_t`, and `entvars_t`.
