#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <thread>

#include "debug_test_common.hpp"
#include "debugging/buffer_sink.hpp"
#include "debugging/json_writer.hpp"
#include "debugging/logging.hpp"
#include "debugging/snapshot_writer.hpp"
#include "debugging/trace.hpp"

using namespace xash::debugging;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SnapshotHeader TestHeader()
{
	SnapshotHeader header = {
		{ "xash3d.debug.test", 1 },
		42,
		1234
	};
	return header;
}

static TraceEvent MakeTraceEvent(uint64_t sequence, const char *message)
{
	static const TraceCategory category = { "test.trace", TraceLevel::Trace };
	TraceEvent event = {
		sequence,
		sequence + 1000,
		TraceLevel::Info,
		&category,
		message
	};
	return event;
}

static bool TestStatusHelpers()
{
	DebugStatus ok = DebugOk();
	DebugStatus failed = MakeDebugStatus(DebugStatusCode::InvalidArgument,
		"bad argument");

	return ok.ok() && !failed.ok() && failed.code == DebugStatusCode::InvalidArgument;
}

static bool TestBufferSink()
{
	char output[16];
	FixedBufferDebugSink sink(output, sizeof(output));

	if (!sink.write(DebugOutputFormat::Human, "abc").ok())
		return false;

	if (!sink.write(DebugOutputFormat::Human, "def").ok())
		return false;

	if (!ExpectString(sink.text(), "abcdef"))
		return false;

	if (sink.length() != 6 || sink.capacity() != sizeof(output))
		return false;

	if (sink.lastFormat() != DebugOutputFormat::Human)
		return false;

	DebugStatus overflow = sink.write(DebugOutputFormat::Human,
		"0123456789abcdef");

	if (overflow.ok() || overflow.code != DebugStatusCode::BufferTooSmall)
		return false;

	sink.clear();
	if (sink.length() != 0 || !ExpectString(sink.text(), ""))
		return false;

	return sink.writeSpan(DebugOutputFormat::Json, "abcdef", 3).ok() &&
		ExpectString(sink.text(), "abc") &&
		sink.lastFormat() == DebugOutputFormat::Json;
}

static bool TestBufferSinkInvalidArguments()
{
	FixedBufferDebugSink nullBufferSink(NULL, 0);
	DebugStatus nullBufferStatus = nullBufferSink.write(DebugOutputFormat::Human, "abc");
	if (nullBufferStatus.ok() || nullBufferStatus.code != DebugStatusCode::InvalidArgument)
		return false;

	char output[8];
	FixedBufferDebugSink sink(output, sizeof(output));
	DebugStatus nullTextStatus = sink.write(DebugOutputFormat::Human, NULL);
	if (nullTextStatus.ok() || nullTextStatus.code != DebugStatusCode::InvalidArgument)
		return false;

	return true;
}

static bool TestJsonEscaping()
{
	char output[128];
	FixedBufferDebugSink sink(output, sizeof(output));
	const char input[] = "quote \" slash \\ newline\n control\001";

	if (!json::WriteEscapedString(sink, input).ok())
		return false;

	return ExpectString(sink.text(),
		"\"quote \\\" slash \\\\ newline\\n control\\u0001\"");
}

static bool TestHumanSnapshotHeader()
{
	char output[128];
	FixedBufferDebugSink sink(output, sizeof(output));

	if (!WriteHumanSnapshotHeader(sink, TestHeader()).ok())
		return false;

	return ExpectString(sink.text(),
		"schema: xash3d.debug.test.v1\n"
		"sequence: 42\n"
		"timestamp_usec: 1234\n");
}

static bool TestJsonSnapshotHeader()
{
	char output[160];
	FixedBufferDebugSink sink(output, sizeof(output));

	if (!WriteJsonSnapshotHeader(sink, TestHeader()).ok())
		return false;

	return ExpectString(sink.text(),
		"{\"schema\":\"xash3d.debug.test\",\"version\":1,"
		"\"sequence\":42,\"timestampUsec\":1234}");
}

static bool TestInvalidHeader()
{
	char output[64];
	FixedBufferDebugSink sink(output, sizeof(output));
	SnapshotHeader header = {
		{ "", 1 },
		0,
		0
	};
	DebugStatus status = WriteJsonSnapshotHeader(sink, header);

	return !status.ok() && status.code == DebugStatusCode::InvalidArgument;
}

static bool TestSinkFailurePropagation()
{
	FailingDebugSink humanSink(DebugStatusCode::SinkUnavailable);
	DebugStatus humanStatus = WriteHumanSnapshotHeader(humanSink, TestHeader());
	if (humanStatus.ok() || humanStatus.code != DebugStatusCode::SinkUnavailable)
		return false;

	FailingDebugSink jsonSink(DebugStatusCode::BufferTooSmall);
	DebugStatus jsonStatus = WriteJsonSnapshotHeader(jsonSink, TestHeader());
	if (jsonStatus.ok() || jsonStatus.code != DebugStatusCode::BufferTooSmall)
		return false;

	FailingDebugSink stringSink(DebugStatusCode::AllocationFailed);
	DebugStatus stringStatus = json::WriteEscapedString(stringSink, "hello");
	if (stringStatus.ok() || stringStatus.code != DebugStatusCode::AllocationFailed)
		return false;

	return humanSink.writeCount() == 1 &&
		jsonSink.writeCount() == 1 &&
		stringSink.writeCount() == 1;
}

static bool TestLargeJsonEscaping()
{
	char input[260];
	char output[272];
	FixedBufferDebugSink sink(output, sizeof(output));

	for (size_t i = 0; i < sizeof(input) - 1; ++i)
		input[i] = 'a';

	input[sizeof(input) - 1] = '\0';

	if (!json::WriteEscapedString(sink, input).ok())
		return false;

	if (sink.length() != sizeof(input) + 1)
		return false;

	if (sink.text()[0] != '"' || sink.text()[sink.length() - 1] != '"')
		return false;

	for (size_t i = 1; i < sink.length() - 1; ++i)
	{
		if (sink.text()[i] != 'a')
			return false;
	}

	return true;
}

static bool TestNullJsonString()
{
	char output[16];
	FixedBufferDebugSink sink(output, sizeof(output));
	DebugStatus status = json::WriteEscapedString(sink, NULL);

	return !status.ok() && status.code == DebugStatusCode::InvalidArgument;
}

static bool TestJsonWriterObjectAndArray()
{
	char output[192];
	FixedBufferDebugSink sink(output, sizeof(output));
	json::JsonWriter writer(sink);

	if (!writer.beginObject().ok())
		return false;

	if (!writer.fieldString("name", "alpha").ok())
		return false;

	if (!writer.fieldUInt("count", 7).ok())
		return false;

	if (!writer.name("items").ok())
		return false;

	if (!writer.beginArray().ok())
		return false;

	if (!writer.stringValue("one").ok())
		return false;

	if (!writer.boolValue(true).ok())
		return false;

	if (!writer.nullValue().ok())
		return false;

	if (!writer.int64Value(-12).ok())
		return false;

	if (!writer.endArray().ok())
		return false;

	if (!writer.endObject().ok())
		return false;

	return writer.depth() == 0 &&
		ExpectString(sink.text(),
			"{\"name\":\"alpha\",\"count\":7,\"items\":[\"one\",true,null,-12]}");
}

static bool TestJsonWriterInvalidState()
{
	char output[64];
	FixedBufferDebugSink sink(output, sizeof(output));
	json::JsonWriter writer(sink);

	DebugStatus missingObject = writer.name("field");
	if (missingObject.ok() || missingObject.code != DebugStatusCode::InvalidArgument)
		return false;

	if (!writer.beginObject().ok())
		return false;

	DebugStatus missingName = writer.stringValue("value");
	if (missingName.ok() || missingName.code != DebugStatusCode::InvalidArgument)
		return false;

	if (!writer.name("field").ok())
		return false;

	DebugStatus missingValue = writer.endObject();
	if (missingValue.ok() || missingValue.code != DebugStatusCode::InvalidArgument)
		return false;

	return true;
}

static bool TestJsonWriterDepthLimit()
{
	char output[64];
	FixedBufferDebugSink sink(output, sizeof(output));
	json::JsonWriter writer(sink);

	for (int i = 0; i < 16; ++i)
	{
		if (!writer.beginArray().ok())
			return false;
	}

	DebugStatus tooDeep = writer.beginArray();
	return !tooDeep.ok() && tooDeep.code == DebugStatusCode::BufferTooSmall &&
		writer.depth() == 16;
}

static bool TestJsonWriterSingleRoot()
{
	char output[32];
	FixedBufferDebugSink sink(output, sizeof(output));
	json::JsonWriter writer(sink);

	if (!writer.stringValue("root").ok())
		return false;

	DebugStatus secondRoot = writer.nullValue();
	return !secondRoot.ok() && secondRoot.code == DebugStatusCode::InvalidArgument &&
		ExpectString(sink.text(), "\"root\"");
}

static bool TestJsonWriterSinkFailure()
{
	FailingDebugSink sink(DebugStatusCode::SinkUnavailable);
	json::JsonWriter writer(sink);
	DebugStatus status = writer.beginObject();

	return !status.ok() && status.code == DebugStatusCode::SinkUnavailable &&
		sink.writeCount() == 1;
}

static bool TestTraceLevelNames()
{
	return ExpectString(TraceLevelName(TraceLevel::Error), "error") &&
		ExpectString(TraceLevelName(TraceLevel::Warn), "warn") &&
		ExpectString(TraceLevelName(TraceLevel::Info), "info") &&
		ExpectString(TraceLevelName(TraceLevel::Debug), "debug") &&
		ExpectString(TraceLevelName(TraceLevel::Verbose), "verbose") &&
		ExpectString(TraceLevelName(TraceLevel::Trace), "trace");
}

static bool TestTraceGate()
{
	const TraceCategory lookup = { "filesystem.lookup", TraceLevel::Debug };
	const TraceCategory disabledCategory = { "", TraceLevel::Trace };
	TraceGate gate;

	if (gate.enabled(lookup, TraceLevel::Error))
		return false;

	gate.setEnabled(true);
	if (!gate.isEnabled())
		return false;

	if (!gate.enabled(lookup, TraceLevel::Error))
		return false;

	if (!gate.enabled(lookup, TraceLevel::Info))
		return false;

	if (gate.enabled(lookup, TraceLevel::Debug))
		return false;

	gate.setRuntimeMaximumLevel(TraceLevel::Trace);
	if (gate.runtimeMaximumLevel() != TraceLevel::Trace)
		return false;

	if (!gate.enabled(lookup, TraceLevel::Debug))
		return false;

	if (gate.enabled(lookup, TraceLevel::Verbose))
		return false;

	if (gate.enabled(disabledCategory, TraceLevel::Error))
		return false;

	return true;
}

static bool TestBoundedTraceQueueDropNewest()
{
	TraceEvent storage[2];
	BoundedTraceQueue queue(storage, 2, TraceOverflowPolicy::DropNewest);

	if (queue.capacity() != 2 || queue.size() != 0 || !queue.empty() ||
		queue.full() || queue.overflowPolicy() != TraceOverflowPolicy::DropNewest)
	{
		return false;
	}

	if (!queue.push(MakeTraceEvent(1, "one")))
		return false;

	if (!queue.push(MakeTraceEvent(2, "two")))
		return false;

	if (!queue.full())
		return false;

	if (queue.push(MakeTraceEvent(3, "three")))
		return false;

	TraceQueueStats stats = queue.stats();
	if (stats.enqueued != 2 || stats.dropped != 1 || queue.size() != 2)
		return false;

	TraceEvent event;
	if (!queue.pop(event) || event.sequence != 1 || !ExpectString(event.message, "one"))
		return false;

	if (!queue.pop(event) || event.sequence != 2 || !ExpectString(event.message, "two"))
		return false;

	if (queue.pop(event))
		return false;

	queue.clear();
	return queue.empty();
}

static bool TestBoundedTraceQueueDropOldest()
{
	TraceEvent storage[2];
	BoundedTraceQueue queue(storage, 2, TraceOverflowPolicy::DropOldest);

	if (!queue.push(MakeTraceEvent(1, "one")))
		return false;

	if (!queue.push(MakeTraceEvent(2, "two")))
		return false;

	if (!queue.push(MakeTraceEvent(3, "three")))
		return false;

	TraceQueueStats stats = queue.stats();
	if (stats.enqueued != 3 || stats.dropped != 1 || queue.size() != 2)
		return false;

	TraceEvent event;
	if (!queue.pop(event) || event.sequence != 2 || !ExpectString(event.message, "two"))
		return false;

	if (!queue.pop(event) || event.sequence != 3 || !ExpectString(event.message, "three"))
		return false;

	return queue.empty();
}

static bool TestBoundedTraceQueueInvalidStorage()
{
	BoundedTraceQueue queue(NULL, 0, TraceOverflowPolicy::DropNewest);
	TraceEvent event = MakeTraceEvent(1, "one");

	if (queue.push(event))
		return false;

	if (queue.capacity() != 0 || queue.size() != 0 || queue.full() || !queue.empty())
		return false;

	TraceQueueStats stats = queue.stats();
	return stats.enqueued == 0 && stats.dropped == 0;
}

static void PushTraceEvents(BoundedTraceQueue *queue, int startSequence, int count)
{
	for (int i = 0; i < count; ++i)
		queue->push(MakeTraceEvent(static_cast<uint64_t>(startSequence + i), "threaded"));
}

static bool TestBoundedTraceQueueConcurrentPushes()
{
	static const int threadCount = 4;
	static const int eventsPerThread = 250;
	static const int totalEvents = threadCount * eventsPerThread;
	TraceEvent storage[32];
	BoundedTraceQueue queue(storage, 32, TraceOverflowPolicy::DropNewest);
	std::thread threads[threadCount];

	for (int i = 0; i < threadCount; ++i)
		threads[i] = std::thread(PushTraceEvents, &queue, i * eventsPerThread, eventsPerThread);

	for (int i = 0; i < threadCount; ++i)
		threads[i].join();

	TraceQueueStats stats = queue.stats();
	if (stats.enqueued + stats.dropped != totalEvents)
		return false;

	if (queue.size() != stats.enqueued)
		return false;

	if (queue.size() > queue.capacity())
		return false;

	TraceEvent event;
	size_t popped = 0;
	while (queue.pop(event))
		++popped;

	return popped == stats.enqueued && queue.empty();
}

static void ToggleTraceGate(TraceGate *gate)
{
	for (int i = 0; i < 1000; ++i)
	{
		gate->setEnabled((i % 2) == 0);
		gate->setRuntimeMaximumLevel((i % 3) == 0 ? TraceLevel::Trace : TraceLevel::Info);
	}
}

static void ReadTraceGate(const TraceGate *gate, const TraceCategory *category,
	std::atomic<int> *reads)
{
	for (int i = 0; i < 1000; ++i)
	{
		(void)gate->enabled(*category, TraceLevel::Info);
		++(*reads);
	}
}

static bool TestTraceGateConcurrentAccess()
{
	const TraceCategory category = { "thread.trace", TraceLevel::Trace };
	TraceGate gate;
	std::atomic<int> reads(0);
	std::thread writer(ToggleTraceGate, &gate);
	std::thread readerA(ReadTraceGate, &gate, &category, &reads);
	std::thread readerB(ReadTraceGate, &gate, &category, &reads);

	writer.join();
	readerA.join();
	readerB.join();

	return reads.load() == 2000;
}

static bool TestLogLevelNames()
{
	return ExpectString(LogLevelName(LogLevel::Error), "error") &&
		ExpectString(LogLevelName(LogLevel::Warn), "warn") &&
		ExpectString(LogLevelName(LogLevel::Info), "info") &&
		ExpectString(LogLevelName(LogLevel::Debug), "debug") &&
		ExpectString(LogLevelName(LogLevel::Trace), "trace");
}

static bool TestLogGate()
{
	const LogCategory category = { "filesystem", LogLevel::Debug };
	const LogCategory emptyCategory = { "", LogLevel::Trace };
	LogGate gate;

	if (!gate.isEnabled())
		return false;

	if (!gate.enabled(category, LogLevel::Error))
		return false;

	if (!gate.enabled(category, LogLevel::Info))
		return false;

	if (gate.enabled(category, LogLevel::Debug))
		return false;

	gate.setRuntimeMaximumLevel(LogLevel::Trace);
	if (gate.runtimeMaximumLevel() != LogLevel::Trace)
		return false;

	if (!gate.enabled(category, LogLevel::Debug))
		return false;

	if (gate.enabled(category, LogLevel::Trace))
		return false;

	if (gate.enabled(emptyCategory, LogLevel::Error))
		return false;

	gate.setEnabled(false);
	if (gate.isEnabled())
		return false;

	return !gate.enabled(category, LogLevel::Error);
}

static bool TestLogger()
{
	const LogCategory category = { "filesystem", LogLevel::Debug };
	RecordingLogSink sink;
	Logger logger;

	DebugStatus missingSink = logger.write(LogLevel::Error, category, "missing sink");
	if (missingSink.ok() || missingSink.code != DebugStatusCode::SinkUnavailable)
		return false;

	logger.setSink(&sink);

	if (!logger.write(LogLevel::Warn, category, "careful", 11, 22).ok())
		return false;

	if (sink.writeCount() != 1)
		return false;

	const LogRecord &record = sink.lastRecord();
	if (record.sequence != 11 || record.timestampUsec != 22 ||
		record.level != LogLevel::Warn || record.category != &category ||
		!ExpectString(record.message, "careful"))
	{
		return false;
	}

	if (!logger.write(LogLevel::Debug, category, "debug disabled").ok())
		return false;

	if (sink.writeCount() != 1)
		return false;

	logger.gate().setRuntimeMaximumLevel(LogLevel::Debug);
	if (!logger.write(LogLevel::Debug, category, "debug enabled").ok())
		return false;

	if (sink.writeCount() != 2)
		return false;

	return true;
}

static bool TestLoggerInvalidInputAndSinkFailure()
{
	const LogCategory category = { "filesystem", LogLevel::Debug };
	const LogCategory emptyCategory = { "", LogLevel::Debug };
	RecordingLogSink sink;
	Logger logger;
	logger.setSink(&sink);

	DebugStatus nullMessage = logger.write(LogLevel::Error, category, NULL);
	if (nullMessage.ok() || nullMessage.code != DebugStatusCode::InvalidArgument)
		return false;

	DebugStatus invalidCategory = logger.write(LogLevel::Error, emptyCategory, "oops");
	if (invalidCategory.ok() || invalidCategory.code != DebugStatusCode::InvalidArgument)
		return false;

	sink.setFail(true);
	DebugStatus sinkFailure = logger.write(LogLevel::Error, category, "sink fails");
	if (sinkFailure.ok() || sinkFailure.code != DebugStatusCode::SinkUnavailable)
		return false;

	return sink.writeCount() == 1;
}

static void ToggleLogGate(LogGate *gate)
{
	for (int i = 0; i < 1000; ++i)
	{
		gate->setEnabled((i % 2) == 0);
		gate->setRuntimeMaximumLevel((i % 3) == 0 ? LogLevel::Trace : LogLevel::Info);
	}
}

static void ReadLogGate(const LogGate *gate, const LogCategory *category,
	std::atomic<int> *reads)
{
	for (int i = 0; i < 1000; ++i)
	{
		(void)gate->enabled(*category, LogLevel::Info);
		++(*reads);
	}
}

static bool TestLogGateConcurrentAccess()
{
	const LogCategory category = { "thread.log", LogLevel::Trace };
	LogGate gate;
	std::atomic<int> reads(0);
	std::thread writer(ToggleLogGate, &gate);
	std::thread readerA(ReadLogGate, &gate, &category, &reads);
	std::thread readerB(ReadLogGate, &gate, &category, &reads);

	writer.join();
	readerA.join();
	readerB.join();

	return reads.load() == 2000;
}

int main()
{
	if (!TestStatusHelpers() ||
		!TestBufferSink() ||
		!TestBufferSinkInvalidArguments() ||
		!TestJsonEscaping() ||
		!TestHumanSnapshotHeader() ||
		!TestJsonSnapshotHeader() ||
		!TestInvalidHeader() ||
		!TestSinkFailurePropagation() ||
		!TestLargeJsonEscaping() ||
		!TestNullJsonString() ||
		!TestJsonWriterObjectAndArray() ||
		!TestJsonWriterInvalidState() ||
		!TestJsonWriterDepthLimit() ||
		!TestJsonWriterSingleRoot() ||
		!TestJsonWriterSinkFailure() ||
		!TestTraceLevelNames() ||
		!TestTraceGate() ||
		!TestBoundedTraceQueueDropNewest() ||
		!TestBoundedTraceQueueDropOldest() ||
		!TestBoundedTraceQueueInvalidStorage() ||
		!TestBoundedTraceQueueConcurrentPushes() ||
		!TestTraceGateConcurrentAccess() ||
		!TestLogLevelNames() ||
		!TestLogGate() ||
		!TestLogger() ||
		!TestLoggerInvalidInputAndSinkFailure() ||
		!TestLogGateConcurrentAccess())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
