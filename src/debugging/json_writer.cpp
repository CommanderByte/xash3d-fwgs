#include "debugging/json_writer.hpp"

namespace xash
{
namespace debugging
{
namespace json
{

namespace
{

DebugStatus WriteRaw(IDebugSink &sink, const char *text)
{
	return sink.write(DebugOutputFormat::Json, text);
}

DebugStatus WriteSpan(IDebugSink &sink, const char *text, size_t length)
{
	char buffer[128];

	while (length > 0)
	{
		size_t chunk = length < sizeof(buffer) - 1 ? length : sizeof(buffer) - 1;

		for (size_t i = 0; i < chunk; ++i)
			buffer[i] = text[i];

		buffer[chunk] = '\0';

		DebugStatus status = WriteRaw(sink, buffer);
		if (!status.ok())
			return status;

		text += chunk;
		length -= chunk;
	}

	return DebugOk();
}

}

DebugStatus WriteEscapedString(IDebugSink &sink, const char *text)
{
	static const char hex[] = "0123456789abcdef";
	DebugStatus status;
	const char *segmentStart;

	if (!text)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON string is null");

	status = WriteRaw(sink, "\"");
	if (!status.ok())
		return status;

	segmentStart = text;

	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p)
	{
		char escaped[7];
		const char *replacement = NULL;

		switch (*p)
		{
		case '"':
			replacement = "\\\"";
			break;
		case '\\':
			replacement = "\\\\";
			break;
		case '\b':
			replacement = "\\b";
			break;
		case '\f':
			replacement = "\\f";
			break;
		case '\n':
			replacement = "\\n";
			break;
		case '\r':
			replacement = "\\r";
			break;
		case '\t':
			replacement = "\\t";
			break;
		default:
			if (*p < 0x20)
			{
				escaped[0] = '\\';
				escaped[1] = 'u';
				escaped[2] = '0';
				escaped[3] = '0';
				escaped[4] = hex[(*p >> 4) & 0x0f];
				escaped[5] = hex[*p & 0x0f];
				escaped[6] = '\0';
				replacement = escaped;
			}
			break;
		}

		if (!replacement)
			continue;

		if (reinterpret_cast<const char *>(p) > segmentStart)
		{
			status = WriteSpan(sink, segmentStart,
				static_cast<size_t>(reinterpret_cast<const char *>(p) - segmentStart));
			if (!status.ok())
				return status;
		}

		status = WriteRaw(sink, replacement);
		if (!status.ok())
			return status;

		segmentStart = reinterpret_cast<const char *>(p + 1);
	}

	if (segmentStart[0])
	{
		status = WriteRaw(sink, segmentStart);
		if (!status.ok())
			return status;
	}

	return WriteRaw(sink, "\"");
}

JsonWriter::JsonWriter(IDebugSink &sink)
	: m_sink(sink)
	, m_depth(0)
	, m_hasRootValue(false)
{
}

DebugStatus JsonWriter::beginObject()
{
	if (m_depth >= sizeof(m_stack) / sizeof(m_stack[0]))
		return MakeDebugStatus(DebugStatusCode::BufferTooSmall, "JSON nesting is too deep");

	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	status = writeRaw("{");
	if (!status.ok())
		return status;

	return push(ContainerKind::Object);
}

DebugStatus JsonWriter::endObject()
{
	return pop(ContainerKind::Object, "}");
}

DebugStatus JsonWriter::beginArray()
{
	if (m_depth >= sizeof(m_stack) / sizeof(m_stack[0]))
		return MakeDebugStatus(DebugStatusCode::BufferTooSmall, "JSON nesting is too deep");

	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	status = writeRaw("[");
	if (!status.ok())
		return status;

	return push(ContainerKind::Array);
}

DebugStatus JsonWriter::endArray()
{
	return pop(ContainerKind::Array, "]");
}

DebugStatus JsonWriter::name(const char *fieldName)
{
	DebugStatus status;

	if (!fieldName || !fieldName[0])
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON field name is empty");

	status = beforeName();
	if (!status.ok())
		return status;

	status = WriteEscapedString(m_sink, fieldName);
	if (!status.ok())
		return status;

	status = writeRaw(":");
	if (!status.ok())
		return status;

	m_stack[m_depth - 1].expectingValue = true;
	return DebugOk();
}

DebugStatus JsonWriter::stringValue(const char *value)
{
	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	return WriteEscapedString(m_sink, value);
}

DebugStatus JsonWriter::uint64Value(uint64_t value)
{
	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	return writeUInt64(value);
}

DebugStatus JsonWriter::int64Value(int64_t value)
{
	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	if (value < 0)
	{
		status = writeRaw("-");
		if (!status.ok())
			return status;

		return writeUInt64(0 - static_cast<uint64_t>(value));
	}

	return writeUInt64(static_cast<uint64_t>(value));
}

DebugStatus JsonWriter::boolValue(bool value)
{
	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	return writeRaw(value ? "true" : "false");
}

DebugStatus JsonWriter::nullValue()
{
	DebugStatus status = beforeValue();
	if (!status.ok())
		return status;

	return writeRaw("null");
}

DebugStatus JsonWriter::fieldString(const char *fieldName, const char *value)
{
	DebugStatus status = name(fieldName);
	if (!status.ok())
		return status;

	return stringValue(value);
}

DebugStatus JsonWriter::fieldUInt64(const char *fieldName, uint64_t value)
{
	DebugStatus status = name(fieldName);
	if (!status.ok())
		return status;

	return uint64Value(value);
}

DebugStatus JsonWriter::fieldUInt(const char *fieldName, unsigned value)
{
	return fieldUInt64(fieldName, static_cast<uint64_t>(value));
}

DebugStatus JsonWriter::fieldInt64(const char *fieldName, int64_t value)
{
	DebugStatus status = name(fieldName);
	if (!status.ok())
		return status;

	return int64Value(value);
}

DebugStatus JsonWriter::fieldBool(const char *fieldName, bool value)
{
	DebugStatus status = name(fieldName);
	if (!status.ok())
		return status;

	return boolValue(value);
}

DebugStatus JsonWriter::fieldNull(const char *fieldName)
{
	DebugStatus status = name(fieldName);
	if (!status.ok())
		return status;

	return nullValue();
}

unsigned JsonWriter::depth() const
{
	return m_depth;
}

DebugStatus JsonWriter::beforeName()
{
	ContainerState *state;

	if (m_depth == 0 || m_stack[m_depth - 1].kind != ContainerKind::Object)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON field name outside object");

	state = &m_stack[m_depth - 1];

	if (state->expectingValue)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON field value is missing");

	if (!state->first)
	{
		DebugStatus status = writeRaw(",");
		if (!status.ok())
			return status;
	}

	state->first = false;
	return DebugOk();
}

DebugStatus JsonWriter::beforeValue()
{
	if (m_depth == 0)
	{
		if (m_hasRootValue)
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON root value already written");

		m_hasRootValue = true;
		return DebugOk();
	}

	ContainerState *state = &m_stack[m_depth - 1];

	if (state->kind == ContainerKind::Object)
	{
		if (!state->expectingValue)
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON field name is missing");

		state->expectingValue = false;
		return DebugOk();
	}

	if (!state->first)
	{
		DebugStatus status = writeRaw(",");
		if (!status.ok())
			return status;
	}

	state->first = false;
	return DebugOk();
}

DebugStatus JsonWriter::writeRaw(const char *text)
{
	return WriteRaw(m_sink, text);
}

DebugStatus JsonWriter::writeUInt64(uint64_t value)
{
	char reversed[32];
	char output[32];
	size_t count = 0;

	if (value == 0)
		return writeRaw("0");

	while (value > 0 && count < sizeof(reversed))
	{
		reversed[count++] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}

	if (count >= sizeof(output))
		return MakeDebugStatus(DebugStatusCode::BufferTooSmall, "integer buffer too small");

	for (size_t i = 0; i < count; ++i)
		output[i] = reversed[count - i - 1];

	output[count] = '\0';
	return writeRaw(output);
}

DebugStatus JsonWriter::push(ContainerKind kind)
{
	if (m_depth >= sizeof(m_stack) / sizeof(m_stack[0]))
		return MakeDebugStatus(DebugStatusCode::BufferTooSmall, "JSON nesting is too deep");

	ContainerState state = {
		kind,
		true,
		false
	};
	m_stack[m_depth++] = state;
	return DebugOk();
}

DebugStatus JsonWriter::pop(ContainerKind kind, const char *terminator)
{
	if (m_depth == 0 || m_stack[m_depth - 1].kind != kind)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON container mismatch");

	if (m_stack[m_depth - 1].expectingValue)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "JSON field value is missing");

	--m_depth;
	return writeRaw(terminator);
}

}
}
}
