#ifndef XASH_DEBUGGING_TRACE_HPP
#define XASH_DEBUGGING_TRACE_HPP

#include <atomic>

#include "debugging/debug_types.hpp"

namespace xash
{
namespace debugging
{

enum class TraceLevel
{
	Error = 0,
	Warn = 1,
	Info = 2,
	Debug = 3,
	Verbose = 4,
	Trace = 5,
};

struct TraceCategory
{
	const char *name;
	TraceLevel compiledMaximumLevel;
};

struct TraceEvent
{
	uint64_t sequence;
	uint64_t timestampUsec;
	TraceLevel level;
	const TraceCategory *category;
	const char *message;
};

struct TraceQueueStats
{
	uint64_t enqueued;
	uint64_t dropped;
};

enum class TraceOverflowPolicy
{
	DropNewest,
	DropOldest,
};

const char *TraceLevelName(TraceLevel level);

class TraceGate
{
public:
	TraceGate();

	bool enabled(const TraceCategory &category, TraceLevel level) const;

	void setEnabled(bool enabled);
	bool isEnabled() const;

	void setRuntimeMaximumLevel(TraceLevel level);
	TraceLevel runtimeMaximumLevel() const;

private:
	std::atomic<bool> m_enabled;
	std::atomic<int> m_runtimeMaximumLevel;
};

class BoundedTraceQueue
{
public:
	BoundedTraceQueue(TraceEvent *storage, size_t capacity,
		TraceOverflowPolicy overflowPolicy);

	bool push(const TraceEvent &event);
	bool pop(TraceEvent &event);

	size_t size() const;
	size_t capacity() const;
	bool empty() const;
	bool full() const;

	TraceOverflowPolicy overflowPolicy() const;
	TraceQueueStats stats() const;

	void clear();

private:
	void Lock() const;
	void Unlock() const;
	size_t Next(size_t index) const;
	bool FullUnlocked() const;

	TraceEvent *m_storage;
	size_t m_capacity;
	size_t m_head;
	size_t m_size;
	TraceOverflowPolicy m_overflowPolicy;
	TraceQueueStats m_stats;
	mutable std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};

}
}

#endif
