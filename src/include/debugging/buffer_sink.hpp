#ifndef XASH_DEBUGGING_BUFFER_SINK_HPP
#define XASH_DEBUGGING_BUFFER_SINK_HPP

#include <stddef.h>
#include <string.h>

#include "debugging/debug_sink.hpp"

namespace xash
{
namespace debugging
{

class FixedBufferDebugSink final : public IDebugSink
{
public:
	FixedBufferDebugSink(char *buffer, size_t capacity)
		: m_buffer(buffer)
		, m_capacity(capacity)
		, m_length(0)
		, m_lastFormat(DebugOutputFormat::Human)
	{
		clear();
	}

	DebugStatus write(DebugOutputFormat format, const char *text) override
	{
		if (!text)
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "buffer sink text is null");

		return writeSpan(format, text, strlen(text));
	}

	DebugStatus writeSpan(DebugOutputFormat format, const char *text, size_t length)
	{
		if (!m_buffer || m_capacity == 0 || !text)
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "buffer sink argument is invalid");

		if (length >= m_capacity - m_length)
			return MakeDebugStatus(DebugStatusCode::BufferTooSmall, "buffer sink is full");

		memcpy(m_buffer + m_length, text, length);
		m_length += length;
		m_buffer[m_length] = '\0';
		m_lastFormat = format;

		return DebugOk();
	}

	void clear()
	{
		m_length = 0;

		if (m_buffer && m_capacity > 0)
			m_buffer[0] = '\0';
	}

	const char *text() const
	{
		return m_buffer ? m_buffer : "";
	}

	size_t length() const
	{
		return m_length;
	}

	size_t capacity() const
	{
		return m_capacity;
	}

	DebugOutputFormat lastFormat() const
	{
		return m_lastFormat;
	}

private:
	char *m_buffer;
	size_t m_capacity;
	size_t m_length;
	DebugOutputFormat m_lastFormat;
};

}
}

#endif
