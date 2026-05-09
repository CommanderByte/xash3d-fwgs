#include "debugging/logging.hpp"

namespace xash
{
namespace debugging
{

namespace
{

bool LevelAllowed(LogLevel requested, LogLevel maximum)
{
	return static_cast<int>(requested) <= static_cast<int>(maximum);
}

bool ValidCategory(const LogCategory *category)
{
	return category && category->name && category->name[0];
}

}

const char *LogLevelName(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Error:
		return "error";
	case LogLevel::Warn:
		return "warn";
	case LogLevel::Info:
		return "info";
	case LogLevel::Debug:
		return "debug";
	case LogLevel::Trace:
		return "trace";
	default:
		return "unknown";
	}
}

LogGate::LogGate()
	: m_enabled(true)
	, m_runtimeMaximumLevel(static_cast<int>(LogLevel::Info))
{
}

bool LogGate::enabled(const LogCategory &category, LogLevel level) const
{
	if (!m_enabled.load(std::memory_order_acquire))
		return false;

	if (!ValidCategory(&category))
		return false;

	const LogLevel runtimeMaximumLevel =
		static_cast<LogLevel>(m_runtimeMaximumLevel.load(std::memory_order_relaxed));

	return LevelAllowed(level, runtimeMaximumLevel) &&
		LevelAllowed(level, category.compiledMaximumLevel);
}

void LogGate::setEnabled(bool enabled)
{
	m_enabled.store(enabled, std::memory_order_release);
}

bool LogGate::isEnabled() const
{
	return m_enabled.load(std::memory_order_acquire);
}

void LogGate::setRuntimeMaximumLevel(LogLevel level)
{
	m_runtimeMaximumLevel.store(static_cast<int>(level), std::memory_order_release);
}

LogLevel LogGate::runtimeMaximumLevel() const
{
	return static_cast<LogLevel>(m_runtimeMaximumLevel.load(std::memory_order_acquire));
}

Logger::Logger()
	: m_sink(NULL)
{
}

void Logger::setSink(ILogSink *sink)
{
	m_sink.store(sink, std::memory_order_release);
}

ILogSink *Logger::sink() const
{
	return m_sink.load(std::memory_order_acquire);
}

LogGate &Logger::gate()
{
	return m_gate;
}

const LogGate &Logger::gate() const
{
	return m_gate;
}

DebugStatus Logger::write(const LogRecord &record)
{
	if (!ValidCategory(record.category))
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "log category is invalid");

	if (!record.message)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "log message is null");

	if (!m_gate.enabled(*record.category, record.level))
		return DebugOk();

	ILogSink *currentSink = m_sink.load(std::memory_order_acquire);
	if (!currentSink)
		return MakeDebugStatus(DebugStatusCode::SinkUnavailable, "log sink is unavailable");

	return currentSink->write(record);
}

DebugStatus Logger::write(LogLevel level, const LogCategory &category,
	const char *message, uint64_t sequence, uint64_t timestampUsec)
{
	LogRecord record = {
		sequence,
		timestampUsec,
		level,
		&category,
		message
	};

	return write(record);
}

}
}
