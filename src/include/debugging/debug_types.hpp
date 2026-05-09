#ifndef XASH_DEBUGGING_DEBUG_TYPES_HPP
#define XASH_DEBUGGING_DEBUG_TYPES_HPP

#include <stddef.h>
#include <stdint.h>

namespace xash
{
namespace debugging
{

struct SnapshotSchema
{
	const char *name;
	unsigned version;
};

struct SnapshotHeader
{
	SnapshotSchema schema;
	uint64_t sequence;
	uint64_t timestampUsec;
};

enum class DebugOutputFormat
{
	Human,
	Json,
};

enum class DebugStatusCode
{
	Ok,
	AllocationFailed,
	InvalidArgument,
	BufferTooSmall,
	SinkUnavailable,
	UnsupportedFormat,
};

struct DebugStatus
{
	DebugStatusCode code;
	const char *message;

	bool ok() const { return code == DebugStatusCode::Ok; }
};

inline DebugStatus MakeDebugStatus(DebugStatusCode code, const char *message)
{
	DebugStatus status = { code, message };
	return status;
}

inline DebugStatus DebugOk()
{
	return MakeDebugStatus(DebugStatusCode::Ok, "");
}

}
}

#endif
