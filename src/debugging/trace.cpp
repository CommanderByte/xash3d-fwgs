#include "debugging/trace.hpp"

namespace xash
{
namespace debugging
{

namespace
{

bool LevelAllowed(TraceLevel requested, TraceLevel maximum)
{
	return static_cast<int>(requested) <= static_cast<int>(maximum);
}

}

const char *TraceLevelName(TraceLevel level)
{
	switch (level)
	{
	case TraceLevel::Error:
		return "error";
	case TraceLevel::Warn:
		return "warn";
	case TraceLevel::Info:
		return "info";
	case TraceLevel::Debug:
		return "debug";
	case TraceLevel::Verbose:
		return "verbose";
	case TraceLevel::Trace:
		return "trace";
	default:
		return "unknown";
	}
}

TraceGate::TraceGate()
	: m_enabled(false)
	, m_runtimeMaximumLevel(static_cast<int>(TraceLevel::Info))
{
}

bool TraceGate::enabled(const TraceCategory &category, TraceLevel level) const
{
	if (!m_enabled.load(std::memory_order_acquire))
		return false;

	if (!category.name || !category.name[0])
		return false;

	const TraceLevel runtimeMaximumLevel =
		static_cast<TraceLevel>(m_runtimeMaximumLevel.load(std::memory_order_relaxed));

	return LevelAllowed(level, runtimeMaximumLevel) &&
		LevelAllowed(level, category.compiledMaximumLevel);
}

void TraceGate::setEnabled(bool enabled)
{
	m_enabled.store(enabled, std::memory_order_release);
}

bool TraceGate::isEnabled() const
{
	return m_enabled.load(std::memory_order_acquire);
}

void TraceGate::setRuntimeMaximumLevel(TraceLevel level)
{
	m_runtimeMaximumLevel.store(static_cast<int>(level), std::memory_order_release);
}

TraceLevel TraceGate::runtimeMaximumLevel() const
{
	return static_cast<TraceLevel>(m_runtimeMaximumLevel.load(std::memory_order_acquire));
}

BoundedTraceQueue::BoundedTraceQueue(TraceEvent *storage, size_t capacity,
	TraceOverflowPolicy overflowPolicy)
	: m_storage(storage)
	, m_capacity(capacity)
	, m_head(0)
	, m_size(0)
	, m_overflowPolicy(overflowPolicy)
{
	m_stats.enqueued = 0;
	m_stats.dropped = 0;
	m_lock.clear(std::memory_order_release);
}

bool BoundedTraceQueue::push(const TraceEvent &event)
{
	if (!m_storage || m_capacity == 0)
		return false;

	Lock();

	if (FullUnlocked())
	{
		++m_stats.dropped;

		if (m_overflowPolicy == TraceOverflowPolicy::DropNewest)
		{
			Unlock();
			return false;
		}

		m_head = Next(m_head);
		--m_size;
	}

	const size_t tail = (m_head + m_size) % m_capacity;
	m_storage[tail] = event;
	++m_size;
	++m_stats.enqueued;
	Unlock();
	return true;
}

bool BoundedTraceQueue::pop(TraceEvent &event)
{
	Lock();

	if (m_size == 0)
	{
		Unlock();
		return false;
	}

	event = m_storage[m_head];
	m_head = Next(m_head);
	--m_size;
	Unlock();
	return true;
}

size_t BoundedTraceQueue::size() const
{
	Lock();
	const size_t currentSize = m_size;
	Unlock();
	return currentSize;
}

size_t BoundedTraceQueue::capacity() const
{
	return m_capacity;
}

bool BoundedTraceQueue::empty() const
{
	return size() == 0;
}

bool BoundedTraceQueue::full() const
{
	Lock();
	const bool isFull = FullUnlocked();
	Unlock();
	return isFull;
}

TraceOverflowPolicy BoundedTraceQueue::overflowPolicy() const
{
	return m_overflowPolicy;
}

TraceQueueStats BoundedTraceQueue::stats() const
{
	Lock();
	const TraceQueueStats currentStats = m_stats;
	Unlock();
	return currentStats;
}

void BoundedTraceQueue::clear()
{
	Lock();
	m_head = 0;
	m_size = 0;
	Unlock();
}

void BoundedTraceQueue::Lock() const
{
	while (m_lock.test_and_set(std::memory_order_acquire))
	{
	}
}

void BoundedTraceQueue::Unlock() const
{
	m_lock.clear(std::memory_order_release);
}

size_t BoundedTraceQueue::Next(size_t index) const
{
	return (index + 1) % m_capacity;
}

bool BoundedTraceQueue::FullUnlocked() const
{
	return m_capacity > 0 && m_size == m_capacity;
}

}
}
