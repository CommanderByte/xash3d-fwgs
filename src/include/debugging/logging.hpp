#ifndef XASH_DEBUGGING_LOGGING_HPP
#define XASH_DEBUGGING_LOGGING_HPP

#include <atomic>

#include "debugging/debug_types.hpp"

namespace xash
{
namespace debugging
{

enum class LogLevel
{
	Error = 0,
	Warn = 1,
	Info = 2,
	Debug = 3,
	Trace = 4,
};

struct LogCategory
{
	const char *name;
	LogLevel compiledMaximumLevel;
};

struct LogRecord
{
	uint64_t sequence;
	uint64_t timestampUsec;
	LogLevel level;
	const LogCategory *category;
	const char *message;
};

const char *LogLevelName(LogLevel level);

class LogGate
{
public:
	LogGate();

	bool enabled(const LogCategory &category, LogLevel level) const;

	void setEnabled(bool enabled);
	bool isEnabled() const;

	void setRuntimeMaximumLevel(LogLevel level);
	LogLevel runtimeMaximumLevel() const;

private:
	std::atomic<bool> m_enabled;
	std::atomic<int> m_runtimeMaximumLevel;
};

class ILogSink
{
public:
	virtual ~ILogSink() = default;
	virtual DebugStatus write(const LogRecord &record) = 0;
};

class Logger
{
public:
	Logger();

	void setSink(ILogSink *sink);
	ILogSink *sink() const;

	LogGate &gate();
	const LogGate &gate() const;

	DebugStatus write(const LogRecord &record);
	DebugStatus write(LogLevel level, const LogCategory &category,
		const char *message, uint64_t sequence = 0, uint64_t timestampUsec = 0);

private:
	std::atomic<ILogSink *> m_sink;
	LogGate m_gate;
};

}
}

#endif
