#ifndef XASH_ENGINE_CONSOLE_PLATFORM_CONSOLE_BACKEND_ADAPTER_H
#define XASH_ENGINE_CONSOLE_PLATFORM_CONSOLE_BACKEND_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

void Xash_Win32Console_Create(int dedicated, int showAlways, int developerLevel);
void Xash_Win32Console_Destroy(void);
void Xash_Win32Console_Print(const char *text);
char *Xash_Win32Console_Input(void);
void Xash_Win32Console_Show(int visible);
void Xash_Win32Console_DisableInput(void);
void Xash_Win32Console_SetStatus(const char *text);
void Xash_Win32Console_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif
