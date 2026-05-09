#ifndef XASH_DEBUGGING_DEBUG_SINK_HPP
#define XASH_DEBUGGING_DEBUG_SINK_HPP

#include "debugging/debug_types.hpp"

namespace xash
{
namespace debugging
{

class IDebugSink
{
public:
	virtual ~IDebugSink() = default;
	virtual DebugStatus write(DebugOutputFormat format, const char *text) = 0;
};

}
}

#endif
