#ifndef XASH_DEBUGGING_SNAPSHOT_WRITER_HPP
#define XASH_DEBUGGING_SNAPSHOT_WRITER_HPP

#include "debugging/debug_sink.hpp"
#include "debugging/json_writer.hpp"

namespace xash
{
namespace debugging
{

DebugStatus WriteHumanSnapshotHeader(IDebugSink &sink,
	const SnapshotHeader &header);

DebugStatus WriteJsonSnapshotHeader(IDebugSink &sink,
	const SnapshotHeader &header);

}
}

#endif
