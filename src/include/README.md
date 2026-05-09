# Private C++ Include Area

This folder is reserved for reusable C++ headers used by modernized internals.

Do not place public SDK or ABI headers here. Headers in this folder should be
private to the engine/fork unless a later decision explicitly promotes them.

Initial candidates:

- status/result helpers
- debug snapshot and serializer helpers
- debugging sink, formatter, and trace helper interfaces
- generic registry helpers
- small platform-neutral utility wrappers
- logging facade declarations
- thread/synchronization facades

Planned subfolders:

- `debugging/`: private contracts for shared debug snapshots, sinks,
  formatters, JSON writers, and trace helpers
- `filesystem/`: private modern filesystem records and helper interfaces
- `launcher/`: private contracts for launcher settings, library loading,
  application sequencing, and argument ownership
- `utilities/`: subsystem-neutral helpers such as the ordered registry
