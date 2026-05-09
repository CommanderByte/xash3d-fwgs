#include "debugging/snapshot_writer.hpp"

namespace xash
{
namespace debugging
{

namespace
{

DebugStatus Write(IDebugSink &sink, DebugOutputFormat format, const char *text)
{
	return sink.write(format, text);
}

DebugStatus WriteUInt64(IDebugSink &sink, DebugOutputFormat format, uint64_t value)
{
	char reversed[32];
	char output[32];
	size_t count = 0;

	if (value == 0)
		return Write(sink, format, "0");

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
	return Write(sink, format, output);
}

DebugStatus WriteUInt(IDebugSink &sink, DebugOutputFormat format, unsigned value)
{
	return WriteUInt64(sink, format, static_cast<uint64_t>(value));
}

DebugStatus ValidateHeader(const SnapshotHeader &header)
{
	if (!header.schema.name || !header.schema.name[0])
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "snapshot schema name is empty");

	if (header.schema.version == 0)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "snapshot schema version is zero");

	return DebugOk();
}

}

DebugStatus WriteHumanSnapshotHeader(IDebugSink &sink,
	const SnapshotHeader &header)
{
	DebugStatus status = ValidateHeader(header);
	if (!status.ok())
		return status;

	status = Write(sink, DebugOutputFormat::Human, "schema: ");
	if (!status.ok())
		return status;

	status = Write(sink, DebugOutputFormat::Human, header.schema.name);
	if (!status.ok())
		return status;

	status = Write(sink, DebugOutputFormat::Human, ".v");
	if (!status.ok())
		return status;

	status = WriteUInt(sink, DebugOutputFormat::Human, header.schema.version);
	if (!status.ok())
		return status;

	status = Write(sink, DebugOutputFormat::Human, "\nsequence: ");
	if (!status.ok())
		return status;

	status = WriteUInt64(sink, DebugOutputFormat::Human, header.sequence);
	if (!status.ok())
		return status;

	status = Write(sink, DebugOutputFormat::Human, "\ntimestamp_usec: ");
	if (!status.ok())
		return status;

	status = WriteUInt64(sink, DebugOutputFormat::Human, header.timestampUsec);
	if (!status.ok())
		return status;

	return Write(sink, DebugOutputFormat::Human, "\n");
}

DebugStatus WriteJsonSnapshotHeader(IDebugSink &sink,
	const SnapshotHeader &header)
{
	DebugStatus status = ValidateHeader(header);
	if (!status.ok())
		return status;

	json::JsonWriter writer(sink);

	status = writer.beginObject();
	if (!status.ok())
		return status;

	status = writer.fieldString("schema", header.schema.name);
	if (!status.ok())
		return status;

	status = writer.fieldUInt("version", header.schema.version);
	if (!status.ok())
		return status;

	status = writer.fieldUInt64("sequence", header.sequence);
	if (!status.ok())
		return status;

	status = writer.fieldUInt64("timestampUsec", header.timestampUsec);
	if (!status.ok())
		return status;

	return writer.endObject();
}

}
}
