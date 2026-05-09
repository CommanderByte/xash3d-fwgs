#include "launcher/launch_settings.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace xash
{
namespace launcher
{

static void CopyConfigString(char *dst, size_t dstSize, const char *src)
{
	if (!dst || !dstSize)
	{
		return;
	}

	if (!src)
	{
		src = "";
	}

	snprintf(dst, dstSize, "%s", src);
	dst[dstSize - 1] = '\0';
}

const char *LauncherConfigFileName()
{
	return "launcher.json";
}

static void SkipWhitespace(const char **cursor)
{
	while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n')
	{
		++(*cursor);
	}
}

static bool ParseLiteral(const char **cursor, const char *literal)
{
	size_t length = strlen(literal);

	if (strncmp(*cursor, literal, length) != 0)
	{
		return false;
	}

	*cursor += length;
	return true;
}

static bool ParseJsonString(const char **cursor, char *dst, size_t dstSize)
{
	size_t written = 0;

	if (**cursor != '"')
	{
		return false;
	}

	++(*cursor);

	while (**cursor && **cursor != '"')
	{
		char c = **cursor;
		++(*cursor);

		if (c == '\\')
		{
			c = **cursor;
			if (!c)
			{
				return false;
			}
			++(*cursor);

			switch (c)
			{
			case '"':
			case '\\':
			case '/':
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			default:
				return false;
			}
		}

		if (dst && dstSize && written + 1 < dstSize)
		{
			dst[written++] = c;
		}
	}

	if (**cursor != '"')
	{
		return false;
	}

	++(*cursor);

	if (dst && dstSize)
	{
		dst[written] = '\0';
	}

	return true;
}

static bool ParseJsonBool(const char **cursor, bool *value)
{
	if (ParseLiteral(cursor, "true"))
	{
		*value = true;
		return true;
	}

	if (ParseLiteral(cursor, "false"))
	{
		*value = false;
		return true;
	}

	return false;
}

static bool ApplyJsonSetting(const char *key, const char *stringValue,
	bool hasBoolValue, bool boolValue, LaunchSettings *settings)
{
	if (strcmp(key, "defaultGameDir") == 0 || strcmp(key, "default_game_dir") == 0)
	{
		if (!stringValue)
		{
			return false;
		}
		CopyConfigString(settings->defaultGameDir, sizeof(settings->defaultGameDir), stringValue);
		return true;
	}

	if (strcmp(key, "engineLibrary") == 0 || strcmp(key, "engine_library") == 0)
	{
		if (!stringValue)
		{
			return false;
		}
		CopyConfigString(settings->engineLibraryName, sizeof(settings->engineLibraryName), stringValue);
		return true;
	}

	if (strcmp(key, "sdl2Library") == 0 || strcmp(key, "sdl2_library") == 0)
	{
		if (!stringValue)
		{
			return false;
		}
		CopyConfigString(settings->sdl2LibraryName, sizeof(settings->sdl2LibraryName), stringValue);
		return true;
	}

	if (strcmp(key, "allowMenuChangeGame") == 0 || strcmp(key, "allow_menu_change_game") == 0)
	{
		if (!hasBoolValue)
		{
			return false;
		}
		settings->allowMenuChangeGame = boolValue;
		return true;
	}

	if (strcmp(key, "disableMenuChangeGame") == 0 || strcmp(key, "disable_menu_change_game") == 0)
	{
		if (!hasBoolValue)
		{
			return false;
		}
		settings->allowMenuChangeGame = !boolValue;
		return true;
	}

	if (strcmp(key, "probeSdl2") == 0 || strcmp(key, "probe_sdl2") == 0)
	{
		if (!hasBoolValue)
		{
			return false;
		}
		settings->probeSdl2Library = boolValue;
		return true;
	}

	return true;
}

bool ApplyLaunchSettingsJson(const char *json, LaunchSettings *settings)
{
	const char *cursor = json;
	LaunchSettings parsedSettings;

	if (!json || !settings)
	{
		return false;
	}

	parsedSettings = *settings;

	SkipWhitespace(&cursor);
	if (*cursor != '{')
	{
		return false;
	}
	++cursor;

	for (;;)
	{
		char key[LauncherStringMax];
		char stringValue[LauncherStringMax];
		bool boolValue = false;
		bool hasStringValue = false;
		bool hasBoolValue = false;

		SkipWhitespace(&cursor);
		if (*cursor == '}')
		{
			++cursor;
			break;
		}

		if (!ParseJsonString(&cursor, key, sizeof(key)))
		{
			return false;
		}

		SkipWhitespace(&cursor);
		if (*cursor != ':')
		{
			return false;
		}
		++cursor;
		SkipWhitespace(&cursor);

		if (*cursor == '"')
		{
			if (!ParseJsonString(&cursor, stringValue, sizeof(stringValue)))
			{
				return false;
			}
			hasStringValue = true;
		}
		else if (ParseJsonBool(&cursor, &boolValue))
		{
			hasBoolValue = true;
		}
		else if (ParseLiteral(&cursor, "null"))
		{
			stringValue[0] = '\0';
			hasStringValue = true;
		}
		else
		{
			return false;
		}

		if (!ApplyJsonSetting(key, hasStringValue ? stringValue : NULL,
			hasBoolValue, boolValue, &parsedSettings))
		{
			return false;
		}

		SkipWhitespace(&cursor);
		if (*cursor == ',')
		{
			++cursor;
			continue;
		}
		if (*cursor == '}')
		{
			++cursor;
			break;
		}
		return false;
	}

	SkipWhitespace(&cursor);
	if (*cursor != '\0')
	{
		return false;
	}

	*settings = parsedSettings;
	return true;
}

bool LoadLaunchSettingsFile(const char *path, LaunchSettings *settings)
{
	FILE *file;
	long length;
	char buffer[8192];

	if (!path || !path[0] || !settings)
	{
		return false;
	}

	file = fopen(path, "rb");
	if (!file)
	{
		return false;
	}

	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return false;
	}

	length = ftell(file);
	if (length < 0 || length >= (long)sizeof(buffer))
	{
		fclose(file);
		return false;
	}

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return false;
	}

	if (fread(buffer, 1, (size_t)length, file) != (size_t)length)
	{
		fclose(file);
		return false;
	}

	fclose(file);
	buffer[length] = '\0';
	return ApplyLaunchSettingsJson(buffer, settings);
}

static bool BuildConfigPathFromArgv0(char *dst, size_t dstSize, int argc, char **argv)
{
	const char *argv0;
	const char *slash;
	const char *backslash;
	const char *separator;
	size_t prefixLength;

	if (!dst || !dstSize || argc <= 0 || !argv || !argv[0] || !argv[0][0])
	{
		return false;
	}

	argv0 = argv[0];
	slash = strrchr(argv0, '/');
	backslash = strrchr(argv0, '\\');

	if (slash && backslash)
	{
		separator = slash > backslash ? slash : backslash;
	}
	else
	{
		separator = slash ? slash : backslash;
	}

	if (!separator)
	{
		return false;
	}

	prefixLength = (size_t)(separator - argv0) + 1;
	if (prefixLength + strlen(LauncherConfigFileName()) + 1 > dstSize)
	{
		return false;
	}

	memcpy(dst, argv0, prefixLength);
	dst[prefixLength] = '\0';
	strncat(dst, LauncherConfigFileName(), dstSize - prefixLength - 1);
	return true;
}

LaunchSettings GetLaunchSettings(int argc, char **argv)
{
	LaunchSettings settings = GetDefaultLaunchSettings();
	const char *envConfig = getenv("XASH3D_LAUNCHER_CONFIG");
	char configPath[LauncherStringMax * 2];

	if (envConfig && envConfig[0] && LoadLaunchSettingsFile(envConfig, &settings))
	{
		return settings;
	}

	if (BuildConfigPathFromArgv0(configPath, sizeof(configPath), argc, argv) &&
		LoadLaunchSettingsFile(configPath, &settings))
	{
		return settings;
	}

	LoadLaunchSettingsFile(LauncherConfigFileName(), &settings);
	return settings;
}

}
}
