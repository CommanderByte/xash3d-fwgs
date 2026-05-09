#include "filesystem/debug_snapshot.hpp"

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

DebugStatus ValidateSnapshot(const FilesystemMountSnapshot &snapshot)
{
	if (!snapshot.header.schema.name || !snapshot.header.schema.name[0])
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem snapshot schema is empty");

	if (snapshot.header.schema.version == 0)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem snapshot schema version is zero");

	if (snapshot.recordCount > 0 && !snapshot.records)
		return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem snapshot records are null");

	for (size_t i = 0; i < snapshot.recordCount; ++i)
	{
		if (!snapshot.records[i].source)
			return MakeDebugStatus(DebugStatusCode::InvalidArgument, "filesystem mount source is null");
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

const char *FilesystemMountTypeName(FilesystemMountType type)
{
	switch (type)
	{
	case FilesystemMountType::Directory:
		return "directory";
	case FilesystemMountType::Pak:
		return "pak";
	case FilesystemMountType::Wad:
		return "wad";
	case FilesystemMountType::Zip:
		return "zip";
	case FilesystemMountType::Pk3Directory:
		return "pk3dir";
	case FilesystemMountType::AndroidAssets:
		return "android_assets";
	case FilesystemMountType::Unknown:
	default:
		return "unknown";
	}
}

DebugStatus WriteHumanMountSnapshot(IDebugSink &sink,
	const FilesystemMountSnapshot &snapshot)
{
	DebugStatus status = ValidateSnapshot(snapshot);
	if (!status.ok())
		return status;

	status = xash::debugging::WriteHumanSnapshotHeader(sink, snapshot.header);
	if (!status.ok())
		return status;

	status = Write(sink, "#  type            flags        write  archive  source\n");
	if (!status.ok())
		return status;

	for (size_t i = 0; i < snapshot.recordCount; ++i)
	{
		const FilesystemMountRecord &record = snapshot.records[i];

		status = WriteUInt(sink, record.order);
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, FilesystemMountTypeName(record.type));
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.flagsText ? record.flagsText : "");
		if (!status.ok())
			return status;

		status = Write(sink, "  ");
		if (!status.ok())
			return status;

		status = Write(sink, record.writable ? "yes" : "no");
		if (!status.ok())
			return status;

		status = Write(sink, "     ");
		if (!status.ok())
			return status;

		status = Write(sink, record.archive ? "yes" : "no");
		if (!status.ok())
			return status;

		status = Write(sink, "      ");
		if (!status.ok())
			return status;

		status = Write(sink, record.source);
		if (!status.ok())
			return status;

		if (record.mountReason && record.mountReason[0])
		{
			status = Write(sink, "  reason=");
			if (!status.ok())
				return status;

			status = Write(sink, record.mountReason);
			if (!status.ok())
				return status;
		}

		if (record.parentArchive && record.parentArchive[0])
		{
			status = Write(sink, "  parent=");
			if (!status.ok())
				return status;

			status = Write(sink, record.parentArchive);
			if (!status.ok())
				return status;
		}

		status = Write(sink, "\n");
		if (!status.ok())
			return status;
	}

	return DebugOk();
}

DebugStatus WriteJsonMountSnapshot(IDebugSink &sink,
	const FilesystemMountSnapshot &snapshot)
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

	status = writer.name("searchPaths");
	if (!status.ok())
		return status;

	status = writer.beginArray();
	if (!status.ok())
		return status;

	for (size_t i = 0; i < snapshot.recordCount; ++i)
	{
		const FilesystemMountRecord &record = snapshot.records[i];

		status = writer.beginObject();
		if (!status.ok())
			return status;

		status = writer.fieldUInt("order", record.order);
		if (!status.ok())
			return status;

		status = writer.fieldString("type", FilesystemMountTypeName(record.type));
		if (!status.ok())
			return status;

		status = writer.fieldString("source", record.source);
		if (!status.ok())
			return status;

		status = writer.fieldUInt("flags", record.flags);
		if (!status.ok())
			return status;

		status = WriteOptionalJsonString(writer, "flagsText", record.flagsText);
		if (!status.ok())
			return status;

		status = writer.fieldBool("writable", record.writable);
		if (!status.ok())
			return status;

		status = writer.fieldBool("archive", record.archive);
		if (!status.ok())
			return status;

		status = WriteOptionalJsonString(writer, "mountReason", record.mountReason);
		if (!status.ok())
			return status;

		status = WriteOptionalJsonString(writer, "parentArchive", record.parentArchive);
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
