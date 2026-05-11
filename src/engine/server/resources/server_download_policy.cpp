#include "engine/server/resources/server_download_policy.hpp"

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

bool StringEmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
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

}
}
}
