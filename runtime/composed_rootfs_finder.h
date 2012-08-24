#ifndef _COMPOSED_ROOTFS_FINDER_H_
#define _COMPOSED_ROOTFS_FINDER_H_

#ifdef OPENWRT_FINDER
static const char *_openwrt_base_dirs[] = { "/rom", "/overlay" };
#define OPENWRT_BASE_DIRS_ARRAY_SIZE (sizeof(_openwrt_base_dirs)/sizeof(_openwrt_base_dirs[0]))
static const size_t _openwrt_base_dirs_len[OPENWRT_BASE_DIRS_ARRAY_SIZE] = { sizeof("/rom") - 1, sizeof("/overlay") - 1 };

static inline int
composed_rootfs_finder_match_file (const char *key, const char *file)
{
    int rc = -1;
    int i;
    for (i = 0; i < OPENWRT_BASE_DIRS_ARRAY_SIZE; i++) {
        rc = strncmp(file, _openwrt_base_dirs[i], _openwrt_base_dirs_len[i]);
        if (rc == 0) {
            rc = strcmp(file + _openwrt_base_dirs_len[i], key);
            if (rc == 0) {
                break;
            }
        }
    }
    return rc;
}
#endif /* OPENWRT_FINDER */


#ifdef BINOS_FINDER

#define BINOS_PACKAGES_RP_BASE_DIR "/tmp/sw/mount/"
#define BINOS_PACKAGES_RP_BASE_DIR_SIZE (sizeof(BINOS_PACKAGES_RP_BASE_DIR) - 1)

static inline int
composed_rootfs_finder_match_file (const char *key, const char *file)
{
  int rc = -1;
  char *file_in_package;
  rc = strncmp(file, BINOS_PACKAGES_RP_BASE_DIR, BINOS_PACKAGES_RP_BASE_DIR_SIZE);
  if (rc == 0) {
    file = file + BINOS_PACKAGES_RP_BASE_DIR_SIZE;
    file_in_package = strchr(file, '/');
    if (file_in_package) {
      rc = strcmp(key, file_in_package);
    } else {
      rc = -1;
    }
  }
  return rc;
}
#endif /* BINOS_FINDER */

#if defined(BINOS_FINDER_FP) || defined(BINOS_FINDER_CC)

#ifdef BINOS_FINDER_FP
#define BINOS_PACKAGES_LC_BASE_DIR "/tmp/sw/fp/"
#else /* BINOS_FINDER_CC */
#define BINOS_PACKAGES_LC_BASE_DIR "/tmp/sw/cc/"
#endif /* BINOS_FINDER_FP */
#define BINOS_PACKAGES_LC_BASE_DIR_SIZE (sizeof(BINOS_PACKAGES_LC_BASE_DIR) - 1)

static inline int
composed_rootfs_finder_match_file (const char *key, const char *file)
{
  int rc = -1;
  char *file_in_package;
  rc = strncmp(file, BINOS_PACKAGES_LC_BASE_DIR, BINOS_PACKAGES_LC_BASE_DIR_SIZE);
  if (rc == 0) {
    file = file + BINOS_PACKAGES_LC_BASE_DIR_SIZE - 1;
    /* Now we need to find 'mount' directory */
    while (file) {
        if ((strncmp(file, "/mount", sizeof("/mount") - 1)) == 0) {
            break;
        }
        file = strchr(file + 1, '/');
    }
    if (file) {
      /* go over /mount directory */
      file_in_package = strchr(file + 1, '/');
      if (file_in_package) {
        rc = strcmp(key, file_in_package);
      } else {
        rc = -1;
      }
    }
  }
  return rc;
}
#endif /* defined(BINOS_FINDER_FP) || defined(BINOS_FINDER_CC) */

#endif /* _COMPOSED_ROOTFS_FINDER_H_ */
