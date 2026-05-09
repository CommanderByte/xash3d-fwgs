#ifndef XASH_ENGINE_PLATFORM_COMMAND_LINE_ADAPTER_H
#define XASH_ENGINE_PLATFORM_COMMAND_LINE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

const char *Xash_ChangeGameCensoredArgument(void);
int Xash_FindCommandLineArgument(int argc, const char **argv, const char *argument);
const char *Xash_FindCommandLineValue(int argc, const char **argv, const char *argument);
int Xash_ShouldCensorChangeGameArgument(const char *argument);

#ifdef __cplusplus
}
#endif

#endif
