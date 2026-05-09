#ifndef XASH_FILESYSTEM_PATH_POLICY_ADAPTER_H
#define XASH_FILESYSTEM_PATH_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int FS_PathPolicy_CheckPath(const char *path, int directPathsEnabled);
const char *FS_PathPolicy_StripDirectRelativePrefix(const char *path);
int FS_PathPolicy_IsWriteMode(const char *mode);

#ifdef __cplusplus
}
#endif

#endif
