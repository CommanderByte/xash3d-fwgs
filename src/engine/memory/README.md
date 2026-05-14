# Engine Memory

Reserved for future helpers around the legacy memory pool system in
`engine/common/zone.c`.

Any work here must preserve allocation diagnostics, sentinel behavior, pool
handles, and the public `Mem_*`/`Z_*` macro expectations.
