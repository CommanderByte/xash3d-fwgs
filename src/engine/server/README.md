# Modern Server Internals

This folder contains target-neutral server helpers extracted from
`engine/server/`.

Files here should avoid including `server.h`. Legacy server globals, command
parsing, file I/O, networking sends, and game DLL callbacks belong in adapter
code until a later compatibility phase changes those boundaries.
