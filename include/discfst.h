#ifndef PVZ_WII_DISCFST_H
#define PVZ_WII_DISCFST_H

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mounts the Wii disc's own DATA partition filesystem at "wiidisc:/",
 * exposed through the standard POSIX devoptab (fopen/fread/opendir/...),
 * exactly like fatInit() already does for sd:/ and usb:/. Once mounted,
 * chdir("wiidisc:/") makes every existing "assets/..." relative path in
 * the codebase resolve straight to the disc, no other code changes
 * needed.
 *
 * EXPERIMENTAL: uses DI_OpenPartition()/DI_Read() (<di/di.h>), which asks
 * IOS itself to derive the partition's title key and hand back already-
 * decrypted data -- this code never touches or embeds any key material
 * itself. libogc's own docs note DI_OpenPartition "will only work on
 * original discs" (i.e. IOS verifies the signature via /dev/es), and a
 * homebrew-built disc's ticket is necessarily fake-signed, so whether
 * this succeeds depends on the IOS/loader environment. See the full
 * explanation in source/discfst.cpp's header comment.
 *
 * Returns true if the partition opened and a plausible Wii FST was
 * found. On failure, check DiscFST_GetStatus() (and, if it reports
 * DISCFST_OPEN_PARTITION_FAILED, DiscFST_GetOpenPartitionError() for the
 * raw IOS/ES return code) to see why.
 */
bool DiscFST_Mount(void);

typedef enum
{
    DISCFST_NOT_ATTEMPTED = 0,       /* DiscFST_Mount() hasn't run (yet) */
    DISCFST_NO_DISC,                 /* DI didn't start up / no disc inserted */
    DISCFST_NO_DATA_PARTITION,       /* partition table had no DATA partition */
    DISCFST_OPEN_PARTITION_FAILED,   /* DI_OpenPartition() rejected it -- see DiscFST_GetOpenPartitionError() */
    DISCFST_BAD_FST,                 /* opened, but the FST didn't look sane (bad magic or bad fields) */
    DISCFST_MOUNTED                  /* success */
} DiscFST_Status;

/* What happened during the most recent DiscFST_Mount() call. */
DiscFST_Status DiscFST_GetStatus(void);

/* The raw DI_OpenPartition() return code from the most recent attempt
 * (0 would mean success; only meaningful when DiscFST_GetStatus() ==
 * DISCFST_OPEN_PARTITION_FAILED, since that's the only failure path that
 * sets it to something nonzero). */
int DiscFST_GetOpenPartitionError(void);

/* Finer detail behind a DISCFST_BAD_FST status -- that one status could
 * mean three quite different things, so this narrows it down:
 *   0 = n/a (status wasn't DISCFST_BAD_FST)
 *   1 = the read itself failed / returned fewer bytes than requested
 *       (DiscFST_GetLastMagic() is repurposed to hold how many bytes were
 *       actually returned in this case)
 *   2 = read fine, but the plain Wii disc magic (0x5D1C9EA3) wasn't at
 *       the expected spot -- DiscFST_GetLastMagic() holds what was there
 *       instead, which is useful to compare by hand
 *   3 = the magic matched (so decryption/addressing is working!), but
 *       the fst_offset/fst_size fields looked implausible --
 *       DiscFST_GetLastFstOffset()/DiscFST_GetLastFstSize() hold the raw
 *       (already x4) values that got rejected
 */
int DiscFST_GetBadFstDetail(void);
unsigned int DiscFST_GetLastMagic(void);
unsigned int DiscFST_GetLastFstOffset(void);
unsigned int DiscFST_GetLastFstSize(void);

/* Raw DI_Read() return code from the most recent failed read (0 would be
 * success; only meaningful when DiscFST_GetBadFstDetail() == 1). */
int DiscFST_GetLastDiReadError(void);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_WII_DISCFST_H */
