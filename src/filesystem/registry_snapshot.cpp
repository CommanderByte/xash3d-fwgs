#include "filesystem/registry_snapshot.hpp"

#include "debugging/json_writer.hpp"
#include "debugging/snapshot_writer.hpp"

namespace xash
{
namespace filesystem
{
namespace debugging
{

namespace
{

using xash::debugging::DebugOk;
using xash::debugging::DebugOutputFormat;
using xash::debugging::DebugStatus;
using xash::debugging::DebugStatusCode;
using xash::debugging::IDebugSink;
using xash::debugging::MakeDebugStatus;

DebugStatus Write(IDebugSink &sink, const char *text)
{
	return sink.write(DebugOutputFormat::Human, text);
}

DebugStatus WriteUInt(IDebugSink &sink, unsigned value)
{
	char reversed[16];
	char output[16];
	size_t count = 0;

	if (value == 0)
		return Write(sink, "0");

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
	return Write(sink, output);
}

DebugStatus WriteInt(IDebugSink &sink, int value)
{
	if (value < 0)
	{
		DebugStatus status = Write(sink, "-");
		if (!status.ok())
			return status;

		return WriteUInt(sink, static_cast<unsigned>(0 - static_cast<unsigned>(value)));
	}

	return WriteUInt(sink, static_cast<unsigned>(value));
}

DebugStatus ValidateSnapshot(const FilesystemRegistrySnapshot &snapshot)
{
	if (!snapshot.header.schema.name || !snapshot.header.schema.name[0])
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry snapshot schema is empty");

	if (snapshot.header.schema.version == 0)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry snapshot schema version is zero");

	if (snapshot.recordCount > 0 && !snapshot.records)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry records are null");

	for (size_t i = 0; i < snapshot.recordCount; ++i)
	{
		if (!snapshot.records[i].extension || !snapshot.records[i].extension[0])
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry extension is empty");

		if (!snapshot.records[i].debugName || !snapshot.records[i].debugName[0])
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry debug name is empty");
	}

	return DebugOk();
}

DebugStatus WriteOptionalJsonString(xash::debugging::json::JsonWriter &writer,
	const char *name, const char *value)
{
	if (!value)
		return writer.fieldNull(name);

	return writer.fieldString(name, value);
}

}

DebugStatus CaptureArchiveRegistrySnapshot(const ArchiveRegistry &registry,
	FilesystemRegistryRecord *records, size_t recordCapacity,
	const xash::debugging::SnapshotHeader &header,
	FilesystemRegistrySnapshot &out)
{
	if (registry.count() > 0 && !records)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry snapshot storage is null");

	if (recordCapacity < registry.count())
		return MakeDebugStatus(DebugStatusCode::BufferTooSmall, "filesystem registry snapshot storage is too small");

	for (size_t i = 0; i < registry.count(); ++i)
	{
		const ArchiveRegistry::Record *record = registry.recordAt(i);
		if (!record)
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem registry record is unavailable");

		records[i].order = static_cast<unsigned>(i);
		records[i].extension = record->entry.extension;
		records[i].backendType = record->entry.backendType;
		records[i].realArchive = record->entry.realArchive;
		records[i].autoMountContainedWads = record->entry.autoMountContainedWads;
		records[i].scanPriority = record->entry.scanPriority;
		records[i].debugName = record->entry.debugName;
		records[i].factoryName = record->entry.factoryName;
	}

	out.header = header;
	out.records = records;
	out.recordCount = registry.count();
	return ValidateSnapshot(out);
}

DebugStatus WriteHumanRegistrySnapshot(IDebugSink &sink,
	const FilesystemRegistrySnapshot &snapshot)
{
	DebugStatus status = ValidateSnapshot(snapshot);
	if (!status.ok())
		return status;

	status = xash::debugging::WriteHumanSnapshotHeader(sink, snapshot.header);
	if (!status.ok())
		return status;

	status = Write(sink, "#  extension  backend  real  wads  priority  factory  name\n");
	if (!status.ok())
		return status;

	for (size_t i = 0; i < snapshot.recordCount; ++i)
	{
		const FilesystemRegistryRecord &record = snapshot.records[i];

		status = WriteUInt(sink, record.order);
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.extension);
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, ArchiveBackendTypeName(record.backendType));
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.realArchive ? "yes" : "no");
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.autoMountContainedWads ? "yes" : "no");
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = WriteInt(sink, record.scanPriority);
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.factoryName ? record.factoryName : "");
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.debugName);
		if (!status.ok())
			return status;

		status = Write(sink, "\n");
		if (!status.ok())
			return status;
	}

	return DebugOk();
}

DebugStatus WriteJsonRegistrySnapshot(IDebugSink &sink,
	const FilesystemRegistrySnapshot &snapshot)
{
	DebugStatus status = ValidateSnapshot(snapshot);
	if (!status.ok())
		return status;

	xash::debugging::json::JsonWriter writer(sink);

	status = writer.beginObject();
	if (!status.ok())
		return status;

	status = writer.fieldString("schema", snapshot.header.schema.name);
	if (!status.ok())
		return status;

	status = writer.fieldUInt("version", snapshot.header.schema.version);
	if (!status.ok())
		return status;

	status = writer.fieldUInt64("sequence", snapshot.header.sequence);
	if (!status.ok())
		return status;

	status = writer.fieldUInt64("timestampUsec", snapshot.header.timestampUsec);
	if (!status.ok())
		return status;

	status = writer.name("archiveFormats");
	if (!status.ok())
		return status;

	status = writer.beginArray();
	if (!status.ok())
		return status;

	for (size_t i = 0; i < snapshot.recordCount; ++i)
	{
		const FilesystemRegistryRecord &record = snapshot.records[i];

		status = writer.beginObject();
		if (!status.ok())
			return status;

		status = writer.fieldUInt("order", record.order);
		if (!status.ok())
			return status;

		status = writer.fieldString("extension", record.extension);
		if (!status.ok())
			return status;

		status = writer.fieldString("backend", ArchiveBackendTypeName(record.backendType));
		if (!status.ok())
			return status;

		status = writer.fieldBool("realArchive", record.realArchive);
		if (!status.ok())
			return status;

		status = writer.fieldBool("autoMountContainedWads", record.autoMountContainedWads);
		if (!status.ok())
			return status;

		status = writer.fieldInt64("scanPriority", record.scanPriority);
		if (!status.ok())
			return status;

		status = WriteOptionalJsonString(writer, "factory", record.factoryName);
		if (!status.ok())
			return status;

		status = writer.fieldString("debugName", record.debugName);
		if (!status.ok())
			return status;

		status = writer.endObject();
		if (!status.ok())
			return status;
	}

	status = writer.endArray();
	if (!status.ok())
		return status;

	return writer.endObject();
}

}
}
}
