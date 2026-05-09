# Engine Host

Reserved for future host lifecycle helpers called by legacy `host.c`.

`Host_Main`, `Host_Frame`, shutdown, and error unwinding are high-risk
coordinators. Do not move them here until their smaller dependencies have
tests and clearer ownership.
