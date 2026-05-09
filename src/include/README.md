# Private C++ Include Area

This folder is reserved for reusable C++ headers used by modernized internals.

Do not place public SDK or ABI headers here. Headers in this folder should be
private to the engine/fork unless a later decision explicitly promotes them.

Initial candidates:

- status/result helpers
- debug snapshot and serializer helpers
- generic registry helpers
- small platform-neutral utility wrappers
- logging facade declarations
- thread/synchronization facades
