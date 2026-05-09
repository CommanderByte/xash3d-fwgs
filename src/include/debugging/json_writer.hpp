#ifndef XASH_DEBUGGING_JSON_WRITER_HPP
#define XASH_DEBUGGING_JSON_WRITER_HPP

#include "debugging/debug_sink.hpp"

namespace xash
{
namespace debugging
{
namespace json
{

DebugStatus WriteEscapedString(IDebugSink &sink, const char *text);

class JsonWriter
{
public:
	explicit JsonWriter(IDebugSink &sink);

	DebugStatus beginObject();
	DebugStatus endObject();

	DebugStatus beginArray();
	DebugStatus endArray();

	DebugStatus name(const char *name);

	DebugStatus stringValue(const char *value);
	DebugStatus uint64Value(uint64_t value);
	DebugStatus int64Value(int64_t value);
	DebugStatus boolValue(bool value);
	DebugStatus nullValue();

	DebugStatus fieldString(const char *name, const char *value);
	DebugStatus fieldUInt64(const char *name, uint64_t value);
	DebugStatus fieldUInt(const char *name, unsigned value);
	DebugStatus fieldInt64(const char *name, int64_t value);
	DebugStatus fieldBool(const char *name, bool value);
	DebugStatus fieldNull(const char *name);

	unsigned depth() const;

private:
	enum class ContainerKind
	{
		Object,
		Array,
	};

	struct ContainerState
	{
		ContainerKind kind;
		bool first;
		bool expectingValue;
	};

	DebugStatus beforeName();
	DebugStatus beforeValue();
	DebugStatus writeRaw(const char *text);
	DebugStatus writeUInt64(uint64_t value);
	DebugStatus push(ContainerKind kind);
	DebugStatus pop(ContainerKind kind, const char *terminator);

	IDebugSink &m_sink;
	ContainerState m_stack[16];
	unsigned m_depth;
	bool m_hasRootValue;
};

}
}
}

#endif
