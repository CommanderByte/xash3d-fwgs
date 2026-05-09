#ifndef XASH_TESTS_DEBUGGING_DEBUG_TEST_COMMON_HPP
#define XASH_TESTS_DEBUGGING_DEBUG_TEST_COMMON_HPP

#include <stddef.h>

#include "debugging/debug_sink.hpp"
#include "debugging/logging.hpp"

class FailingDebugSink final : public xash::debugging::IDebugSink
{
public:
	FailingDebugSink(xash::debugging::DebugStatusCode code)
		: m_code(code)
		, m_writeCount(0)
	{
	}

	xash::debugging::DebugStatus write(xash::debugging::DebugOutputFormat format,
		const char *text) override
	{
		(void)format;
		(void)text;

		++m_writeCount;
		return xash::debugging::MakeDebugStatus(m_code, "intentional sink failure");
	}

	size_t writeCount() const
	{
		return m_writeCount;
	}

private:
	xash::debugging::DebugStatusCode m_code;
	size_t m_writeCount;
};

class RecordingLogSink final : public xash::debugging::ILogSink
{
public:
	RecordingLogSink()
		: m_writeCount(0)
		, m_fail(false)
	{
		m_lastRecord.sequence = 0;
		m_lastRecord.timestampUsec = 0;
		m_lastRecord.level = xash::debugging::LogLevel::Info;
		m_lastRecord.category = NULL;
		m_lastRecord.message = NULL;
	}

	xash::debugging::DebugStatus write(const xash::debugging::LogRecord &record) override
	{
		++m_writeCount;

		if (m_fail)
		{
			return xash::debugging::MakeDebugStatus(
				xash::debugging::DebugStatusCode::SinkUnavailable,
				"intentional log sink failure");
		}

		m_lastRecord = record;
		return xash::debugging::DebugOk();
	}

	void setFail(bool fail)
	{
		m_fail = fail;
	}

	size_t writeCount() const
	{
		return m_writeCount;
	}

	const xash::debugging::LogRecord &lastRecord() const
	{
		return m_lastRecord;
	}

private:
	size_t m_writeCount;
	bool m_fail;
	xash::debugging::LogRecord m_lastRecord;
};

#endif
