#include "engine/server/resources/resource_transfer_manifest.hpp"
#include "engine/server/resources/server_resource_catalog.hpp"
#include "engine/server/resources/server_download_policy.hpp"
#include "engine/server/resources/server_upload_queue.hpp"
#include "engine/server/resources/server_hot_resource.hpp"
#include "engine/server/resources/server_reslist_policy.hpp"

#include "utilities/path.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool ResourceDescriptorCanEnterManifest(const ResourceDescriptor &resource)
{
	return resource.name && resource.name[0] != '\0' &&
		resource.type != ResourceType::Unknown;
}

bool StringEmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

bool IsSentenceSoundName(const char *name)
{
	return name && name[0] == '!';
}

bool IsInlineModelName(const char *name)
{
	return name && name[0] == '*';
}

char ToLowerAscii(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value + 'a' - 'A');

	return value;
}

bool EqualsCaseInsensitiveAscii(const char *lhs, const char *rhs)
{
	if (!lhs || !rhs)
		return false;

	while (*lhs && *rhs)
	{
		if (ToLowerAscii(*lhs) != ToLowerAscii(*rhs))
			return false;

		++lhs;
		++rhs;
	}

	return *lhs == *rhs;
}

bool IsModelDownloadName(const char *name)
{
	return EqualsCaseInsensitiveAscii(xash::utilities::FileExtension(name), "mdl");
}

ResourceCatalogEntry SkipEntry()
{
	ResourceCatalogEntry entry = {};
	entry.resource.type = ResourceType::Unknown;
	return entry;
}

ResourceCatalogEntry AddEntry(
	ResourceType type,
	const char *name,
	int index,
	int downloadSize,
	unsigned int flags)
{
	ResourceCatalogEntry entry = {};
	entry.shouldAdd = true;
	entry.resource.name = name;
	entry.resource.type = type;
	entry.resource.index = index;
	entry.resource.downloadSize = downloadSize;
	entry.resource.flags = flags;
	return entry;
}

ServerDownloadDecision MakeDecision(ServerDownloadAction action, const char *fileName = nullptr)
{
	ServerDownloadDecision decision = {};
	decision.action = action;
	decision.resourceIndex = -1;
	decision.fileName = fileName;
	decision.modelTextureName = nullptr;
	return decision;
}

int FindRegularDownloadResourceIndex(const ServerDownloadRequest &request)
{
	const char *name = request.requestedName;

	if (StringEmptyOrNull(name) ||
		!IsSafeDownloadName(name) ||
		!request.allowDownload ||
		name[0] == '!' ||
		!request.sendResources)
	{
		return -1;
	}

	return FindResourceForDownloadName(
		request.resources,
		request.resourceCount,
		name);
}

bool HasFlag(unsigned int flags, unsigned int flag)
{
	return (flags & flag) != 0;
}

bool IsDecal(const ResourceDescriptor &resource)
{
	return resource.type == ResourceType::Decal;
}

HotResourceFileSizeQuery SkipFileSizeQuery()
{
	HotResourceFileSizeQuery query = {};
	return query;
}

HotResourceFileSizeQuery NoFileSizeQuery()
{
	HotResourceFileSizeQuery query = {};
	query.shouldAnnounce = true;
	return query;
}

HotResourceFileSizeQuery FileSizeQuery(const std::string &path)
{
	HotResourceFileSizeQuery query = {};
	query.shouldAnnounce = true;
	query.needsFileSize = true;
	query.path = path;
	return query;
}

HotResourceAnnouncement SkipAnnouncement()
{
	HotResourceAnnouncement announcement = {};
	announcement.resource.type = ResourceType::Unknown;
	return announcement;
}

bool HotResourceNeedsFileSize(ResourceType type, const char *name)
{
	if (StringEmptyOrNull(name))
		return false;

	if (type == ResourceType::Model && IsInlineModelName(name))
		return false;

	return true;
}

void NormalizeSlashes(std::string &path)
{
	for (char &ch : path)
	{
		if (ch == '\\')
			ch = '/';
	}

	std::string compact;
	compact.reserve(path.size());

	for (std::size_t i = 0; i < path.size(); ++i)
	{
		if (path[i] == '/' && i + 1 < path.size() && path[i + 1] == '/')
			continue;

		compact.push_back(path[i]);
	}

	path.swap(compact);
}

bool StartsWith(const std::string &value, const char *prefix)
{
	const std::size_t prefixLength = std::char_traits<char>::length(prefix);
	return value.size() >= prefixLength &&
		value.compare(0, prefixLength, prefix) == 0;
}

ReslistTokenDecision SkipDecision()
{
	ReslistTokenDecision decision = {};
	decision.type = ResourceType::Unknown;
	decision.route = ReslistRoute::Skip;
	return decision;
}

}

void ResourceTransferManifest::clear()
{
	resources_.clear();
}

bool ResourceTransferManifest::empty() const
{
	return resources_.empty();
}

std::size_t ResourceTransferManifest::size() const
{
	return resources_.size();
}

bool ResourceTransferManifest::append(const ResourceDescriptor &resource)
{
	if (!ResourceDescriptorCanEnterManifest(resource))
		return false;

	resources_.push_back(resource);
	return true;
}

bool ResourceTransferManifest::append(const ResourceCatalogEntry &entry)
{
	if (!entry.shouldAdd)
		return false;

	return append(entry.resource);
}

std::size_t ResourceTransferManifest::appendAll(
	const ResourceCatalogEntry *entries,
	std::size_t count)
{
	std::size_t added = 0;

	if (!entries)
		return added;

	for (std::size_t i = 0; i < count; ++i)
	{
		if (append(entries[i]))
			++added;
	}

	return added;
}

const ResourceDescriptor *ResourceTransferManifest::data() const
{
	return resources_.empty() ? nullptr : resources_.data();
}

const ResourceDescriptor *ResourceTransferManifest::resourceAt(std::size_t index) const
{
	if (index >= resources_.size())
		return nullptr;

	return &resources_[index];
}

int ResourceTransferManifest::findDownloadIndex(const char *downloadName) const
{
	return FindResourceForDownloadName(data(), resources_.size(), downloadName);
}

ResourceSizeSummary ResourceTransferManifest::sizeSummary() const
{
	ResourceSizeSummary summary = EmptyResourceSizeSummary();

	for (const ResourceDescriptor &resource : resources_)
		AddResourceToSizeSummary(summary, resource);

	return summary;
}

ServerDownloadRequest ResourceTransferManifest::buildDownloadRequest(
	const char *requestedName,
	bool allowDownload,
	bool sendResources,
	bool sendLogos,
	const char *modelTextureName,
	bool modelTextureAvailable) const
{
	ServerDownloadRequest request = {};
	request.requestedName = requestedName;
	request.allowDownload = allowDownload;
	request.sendResources = sendResources;
	request.sendLogos = sendLogos;
	request.resources = data();
	request.resourceCount = resources_.size();
	request.modelTextureName = modelTextureName;
	request.modelTextureAvailable = modelTextureAvailable;
	return request;
}

bool ResourceTransferManifest::buildResourceMessageRow(
	std::size_t index,
	ResourceMessageRow &row,
	const std::uint8_t *reservedData) const
{
	const ResourceDescriptor *resource = resourceAt(index);
	if (!resource)
	{
		row = {};
		row.type = ResourceType::Unknown;
		return false;
	}

	row = BuildResourceTransferMessageRow(*resource, reservedData);
	return true;
}

ResourceMessageRow BuildResourceTransferMessageRow(
	const ResourceDescriptor &resource,
	const std::uint8_t *reservedData)
{
	ResourceMessageRow row = {};
	row.type = resource.type;
	row.name = resource.name;
	row.index = resource.index;
	row.downloadSize = resource.downloadSize;
	row.flags = resource.flags;
	row.md5Hash = resource.md5Hash;
	row.reservedData = reservedData;
	return row;
}

void ResetResourceCatalogState(ResourceCatalogState &state)
{
	state.soundSentenceMarkerAdded = false;
}

bool ResourceCatalogNeedsFileSize(ResourceType type, const char *name)
{
	if (StringEmptyOrNull(name))
		return false;

	switch (type)
	{
	case ResourceType::Generic:
	case ResourceType::EventScript:
		return true;
	case ResourceType::Sound:
		return !IsSentenceSoundName(name);
	case ResourceType::Model:
		return !IsInlineModelName(name);
	case ResourceType::Decal:
	case ResourceType::Skin:
	case ResourceType::World:
	case ResourceType::Unknown:
	default:
		return false;
	}
}

ResourceCatalogEntry BuildGenericResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	return AddEntry(
		ResourceType::Generic,
		name,
		index,
		probedDownloadSize,
		kResourceCatalogFlagFatalIfMissing);
}

ResourceCatalogEntry BuildSoundResourceCatalogEntry(
	ResourceCatalogState &state,
	const char *name,
	int index,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	if (IsSentenceSoundName(name))
	{
		if (state.soundSentenceMarkerAdded)
			return SkipEntry();

		state.soundSentenceMarkerAdded = true;
		return AddEntry(
			ResourceType::Sound,
			"!",
			index,
			0,
			kResourceCatalogFlagFatalIfMissing);
	}

	return AddEntry(ResourceType::Sound, name, index, probedDownloadSize, 0);
}

ResourceCatalogEntry BuildModelResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize,
	unsigned int flags)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	const int downloadSize =
		ResourceCatalogNeedsFileSize(ResourceType::Model, name) ? probedDownloadSize : 0;

	return AddEntry(ResourceType::Model, name, index, downloadSize, flags);
}

ResourceCatalogEntry BuildDecalResourceCatalogEntry(const char *name, int index)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	return AddEntry(ResourceType::Decal, name, index, 0, 0);
}

ResourceCatalogEntry BuildEventScriptResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	return AddEntry(
		ResourceType::EventScript,
		name,
		index,
		probedDownloadSize,
		kResourceCatalogFlagFatalIfMissing);
}

bool ServerDownloadNeedsModelTextureProbe(const ServerDownloadRequest &request)
{
	return FindRegularDownloadResourceIndex(request) >= 0 &&
		IsModelDownloadName(request.requestedName);
}

ServerDownloadDecision DecideServerDownload(const ServerDownloadRequest &request)
{
	const char *name = request.requestedName;

	if (StringEmptyOrNull(name))
		return MakeDecision(ServerDownloadAction::Ignore);

	if (!IsSafeDownloadName(name) || !request.allowDownload)
		return MakeDecision(ServerDownloadAction::Reject, name);

	if (name[0] != '!')
	{
		if (!request.sendResources)
			return MakeDecision(ServerDownloadAction::Reject, name);

		const int resourceIndex = FindRegularDownloadResourceIndex(request);

		if (resourceIndex < 0)
			return MakeDecision(ServerDownloadAction::Reject, name);

		ServerDownloadDecision decision = MakeDecision(ServerDownloadAction::SendFile, name);
		decision.resourceIndex = resourceIndex;

		if (IsModelDownloadName(name) &&
			request.modelTextureAvailable &&
			!StringEmptyOrNull(request.modelTextureName))
		{
			decision.action = ServerDownloadAction::SendFileWithModelTexture;
			decision.modelTextureName = request.modelTextureName;
		}

		return decision;
	}

	if (!request.sendLogos)
		return MakeDecision(ServerDownloadAction::Reject, name);

	ServerDownloadDecision decision = MakeDecision(ServerDownloadAction::LookupCustomLogo, name);
	if (!ParseCustomMd5ResourceName(name, decision.customHash))
		return MakeDecision(ServerDownloadAction::Reject, name);

	return decision;
}

bool ClientUploadResourceDescriptorIsValid(const ResourceDescriptor &resource)
{
	return resource.type != ResourceType::Unknown &&
		resource.downloadSize <= kMaxClientResourceUploadSize;
}

bool ResourceListUpdateIsTooSoon(double now, double nextAllowedTime)
{
	return now < nextAllowedTime;
}

bool ResourceShouldEstimateUploadNeed(const ResourceDescriptor &resource)
{
	return IsDecal(resource);
}

UploadEstimateDecision DecideUploadEstimate(
	const ResourceDescriptor &resource,
	bool hpakContainsResource)
{
	UploadEstimateDecision decision = {};
	decision.action = UploadEstimateAction::Ignore;

	if (!ResourceShouldEstimateUploadNeed(resource) || hpakContainsResource)
		return decision;

	if (resource.downloadSize != 0)
	{
		decision.action = UploadEstimateAction::MarkMissing;
		decision.uploadSize = resource.downloadSize;
		return decision;
	}

	decision.action = UploadEstimateAction::MissingZeroSize;
	return decision;
}

bool UploadTotalExceedsLimit(int totalBytes, double maxUploadMiB)
{
	return static_cast<double>(totalBytes) > maxUploadMiB * 1024.0 * 1024.0;
}

bool UploadBatchNeedsCustomDataProbe(const ResourceDescriptor &resource)
{
	return HasFlag(resource.flags, kUploadResourceFlagWasMissing) &&
		IsDecal(resource) &&
		HasFlag(resource.flags, kUploadResourceFlagCustom);
}

UploadBatchAction DecideUploadBatchAction(
	const ResourceDescriptor &resource,
	bool customResourceDataExists,
	bool allowUpload)
{
	if (!HasFlag(resource.flags, kUploadResourceFlagWasMissing))
		return UploadBatchAction::MoveToOnHand;

	if (!IsDecal(resource))
		return UploadBatchAction::KeepInNeeded;

	if (!HasFlag(resource.flags, kUploadResourceFlagCustom))
		return UploadBatchAction::ReportNonCustomAndMove;

	if (customResourceDataExists || !allowUpload)
		return UploadBatchAction::MoveToOnHand;

	return UploadBatchAction::RequestCustomUpload;
}

HotResourceFileSizeQuery BuildHotResourceFileSizeQuery(
	const HotResourceRequest &request)
{
	if (StringEmptyOrNull(request.name))
		return SkipFileSizeQuery();

	if (!HotResourceNeedsFileSize(request.type, request.name))
		return NoFileSizeQuery();

	if (request.type == ResourceType::Sound)
		return FileSizeQuery(std::string(kHotResourceSoundPathPrefix) + request.name);

	return FileSizeQuery(request.name ? request.name : "");
}

HotResourceAnnouncement BuildHotResourceAnnouncement(
	const HotResourceRequest &request,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(request.name))
		return SkipAnnouncement();

	HotResourceAnnouncement announcement = {};
	announcement.shouldAnnounce = true;
	announcement.resource.name = request.name;
	announcement.resource.type = request.type;
	announcement.resource.index = request.index;
	announcement.resource.downloadSize =
		HotResourceNeedsFileSize(request.type, request.name) ? probedDownloadSize : 0;
	announcement.resource.flags = request.flags;
	return announcement;
}

ReslistTokenProbe BuildReslistTokenProbe(const char *token)
{
	ReslistTokenProbe probe = {};

	if (StringEmptyOrNull(token) || !IsSafeDownloadName(token))
		return probe;

	probe.safe = true;
	probe.normalizedPath = token;
	NormalizeSlashes(probe.normalizedPath);
	probe.soundPathCandidate = StartsWith(probe.normalizedPath, kReslistSoundPrefix);
	return probe;
}

ReslistTokenDecision BuildReslistTokenDecision(
	const ReslistTokenProbe &probe,
	bool soundFormatSupported)
{
	if (!probe.safe)
		return SkipDecision();

	ReslistTokenDecision decision = {};
	decision.shouldIndex = true;
	decision.normalizedPath = probe.normalizedPath;

	if (probe.soundPathCandidate && soundFormatSupported)
	{
		decision.type = ResourceType::Sound;
		decision.route = ReslistRoute::SoundIndex;
		decision.indexPath = probe.normalizedPath.substr(
			std::char_traits<char>::length(kReslistSoundPrefix));
		return decision;
	}

	decision.type = ResourceType::Generic;
	decision.route = ReslistRoute::GenericIndex;
	decision.indexPath = probe.normalizedPath;
	return decision;
}

ReslistTokenDecision ClassifyReslistToken(
	const char *token,
	bool soundFormatSupported)
{
	return BuildReslistTokenDecision(
		BuildReslistTokenProbe(token),
		soundFormatSupported);
}

}
}
}
