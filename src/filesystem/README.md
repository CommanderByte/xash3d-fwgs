# Future Filesystem Internals

This folder is reserved for future filesystem C++ implementation files.

The first filesystem pilot should still begin as an adapter behind the existing
`filesystem/` module so behavior can be compared easily. Once that approach is
proven, reusable pieces such as backend interfaces, registry helpers, path
policy helpers, and debug snapshot builders may move here.

