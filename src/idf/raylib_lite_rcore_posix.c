// SPDX-License-Identifier: Apache-2.0
/* Upstream rcore references access() and mkdir() for FileExists() and
 * MakeDirectory(). Weak fallbacks keep firmware linkable when the VFS does
 * not provide them; a VFS definition always wins. */
#include <sys/stat.h>
#include <sys/types.h>

__attribute__((weak)) int access(const char *path, int mode)
{
    (void)path;
    (void)mode;
    return -1;
}

__attribute__((weak)) int mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return -1;
}
