/****************************************************************************
 * discfst.cpp -- minimal read-only devoptab for a Wii disc's own DATA
 * partition filesystem (FST), mounted at "wiidisc:/".
 *
 * STATUS: EXPERIMENTAL -- may or may not work depending on IOS/ES
 * behavior against a homebrew (fake-signed) disc. Read on for why.
 * -------------------------------------------------------------------------
 * Two earlier approaches were tried and ruled out first:
 *  1. libogc's libiso9660 -- only mounts a plain-ISO9660 disc, which the
 *     .iso/.wbfs this project builds via WIT is NOT (WIT assembles a
 *     genuine Wii-format disc with its own boot.bin/apploader/FST).
 *  2. Reading the disc's raw sectors directly and doing the decryption
 *     avoidance ourselves (by having WIT write the data partition
 *     unencrypted). This doesn't work: the Wii's DI hardware (and
 *     Dolphin's accurate emulation of it) unconditionally decrypts a
 *     hashed partition's data on every read that goes through the normal
 *     DI/boot path, including the very first load of main.dol by the
 *     apploader -- so an intentionally-unencrypted disc corrupts
 *     everything read through that pipeline instead of avoiding
 *     encryption. Confirmed in practice (black screen at boot).
 *
 * This version uses a different, higher-level libogc API instead:
 * DI_OpenPartition() + DI_Read() (<di/di.h>). DI_OpenPartition() asks IOS
 * itself (via /dev/es) to open a partition and derive its title key --
 * using the key material IOS already holds internally, the exact same way
 * the apploader gets main.dol decrypted at boot. This code never touches,
 * derives, or embeds that key material itself; it only calls the
 * system's own sanctioned API for it, the same call any legitimate disc
 * title (including retail games streaming extra content off their own
 * disc at runtime) uses. Once a partition is open, DI_Read() returns
 * fully decrypted, de-hashed logical bytes directly -- no manual
 * hash-cluster math needed at all (IOS does that internally too).
 *
 * THE OPEN QUESTION, why this is still marked experimental: libogc's own
 * documentation states plainly that DI_OpenPartition "will only work on
 * original discs" -- meaning IOS's /dev/es verifies the partition's
 * signature as part of opening it, and a WIT-built homebrew disc's
 * ticket/TMD is necessarily fake-signed (only Nintendo can produce a
 * genuine signature). Whether that verification actually rejects a
 * fake-signed disc here depends on the IOS in use (some are patched to
 * skip it -- that's the long-standing "trucha bug" homebrew relies on
 * for exactly this kind of thing) and on how closely Dolphin's ES
 * emulation mirrors that check. If DI_OpenPartition fails,
 * DiscFST_Mount() records why (DiscFST_GetStatus() /
 * DiscFST_GetOpenPartitionError(), surfaced as an on-screen debug line
 * next to the missing-texture counter -- see render.cpp) and returns
 * false cleanly; assets then still need to come from SD/USB, same as
 * before. This is genuinely being tested to find out, not assumed to
 * work.
 ****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/dir.h>
#include <sys/iosupport.h>
#include <sys/stat.h>

#include <ogcsys.h>
#include <ogc/disc_io.h>
#include <di/di.h>

#include "discfst.h"

#define WII_SECTOR_SIZE   0x800
#define READ_BUFFER_SIZE  0x8000   /* must be a multiple of WII_SECTOR_SIZE
                                    * and of 32 (DI_Read()'s alignment rule) */

#define PARTITION_TABLE_INFO_OFFSET 0x40000  /* volume-group table, 4 groups */
#define PARTITION_TYPE_DATA         0
#define MAX_PARTITIONS_PER_GROUP    64        /* sanity cap, real discs have 1-3 */

#define WII_DISC_MAGIC    0x5D1C9EA3u
#define FST_ENTRY_SIZE    12
#define FST_MAX_SIZE      (1 * 1024 * 1024)   /* sanity cap -- generous for
                                                * this project's actual
                                                * asset count; kept smaller
                                                * than before to reduce
                                                * worst-case memory pressure
                                                * while chasing the "all
                                                * text disappeared" report */
#define DFST_MAX_NAME     255
#define DFST_MAX_PATH     512

namespace {

DiscFST_Status g_status = DISCFST_NOT_ATTEMPTED;
int g_openPartitionError = 0;

/* Finer-grained detail for the DISCFST_BAD_FST case specifically --
 * "opened but FST looked wrong" was too coarse to tell which of three
 * very different problems it actually was. */
enum LoadFstDetail
{
    DISCFST_DETAIL_NONE = 0,
    DISCFST_DETAIL_READ_FAILED,
    DISCFST_DETAIL_BAD_MAGIC,
    DISCFST_DETAIL_BAD_FST_FIELDS
};
LoadFstDetail g_loadFstDetail = DISCFST_DETAIL_NONE;
u32 g_lastMagicRead = 0;      /* also repurposed to hold the byte count actually
                                * read, when g_loadFstDetail == READ_FAILED */
u32 g_lastFstOffset = 0;
u32 g_lastFstSize = 0;
int g_lastDiReadError = 0; /* raw DI_Read() return code from the most recent failure */

struct DFST_Mount
{
    const DISC_INTERFACE* disc;

    /* Raw, pre-partition cache -- only used to read the outer partition
     * table at PARTITION_TABLE_INFO_OFFSET, which sits outside any
     * partition and is never encrypted/hashed. */
    u8  rawBuffer[READ_BUFFER_SIZE] __attribute__((aligned(32)));
    u32 rawCacheStartSector;
    u32 rawCacheSectors;

    /* Partition-relative cache -- everything from boot.bin onward, read
     * via DI_Read() after a successful DI_OpenPartition(). Offsets here
     * are plain logical byte positions in the already-decrypted,
     * already-de-hashed stream IOS hands back; there's no separate
     * "dataOffset" to track anymore (logical offset 0 == start of
     * boot.bin, directly). */
    u8  partBuffer[READ_BUFFER_SIZE] __attribute__((aligned(32)));
    u64 partCacheStart;   /* logical byte offset currently buffered */
    u32 partCacheLen;     /* valid bytes in partBuffer, 0 == empty */

    u8* fst;           /* malloc'd: entry table immediately followed by
                         * the null-terminated name string table */
    u32 fstEntryCount;
    u32 fstSize;       /* total malloc'd size of fst[] -- used to bounds-
                         * check name offsets and never trust a raw disc
                         * value as an unchecked pointer/index */
};

struct DFST_File
{
    u32 fileOffset;   /* LOGICAL offset (see ReadPartition) within the
                       * partition's unhashed data stream -- NOT an
                       * absolute disc byte offset */
    u32 fileSize;
    u32 cursor;
    bool inUse;
    DFST_Mount* mount;
};

struct DFST_DirState
{
    u32 index;        /* next FST entry index to report */
    u32 rangeEnd;      /* exclusive end of this directory's child range */
    bool inUse;
    DFST_Mount* mount;
};

struct DFST_Lookup
{
    bool isDir;
    u32  index;        /* FST entry index (0 == root) */
    u32  childStart;    /* valid if isDir */
    u32  childEnd;      /* valid if isDir */
    u32  fileOffset;    /* logical (see ReadPartition); valid if !isDir */
    u32  fileSize;      /* valid if !isDir */
};

inline u32 Be32(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

inline u32 Be24(const u8* p)
{
    return ((u32)p[0] << 16) | ((u32)p[1] << 8) | (u32)p[2];
}

/* Single cache-fill read from the RAW, pre-partition disc (used only for
 * the outer partition table), capped to what's currently buffered --
 * mirrors libiso9660's own __read()/_read() split for the same
 * __io_wiidvd disc interface. Returns bytes actually copied (<= len), or
 * -1 on a hardware read failure. */
int ReadRaw(DFST_Mount* m, void* ptr, u64 offset, size_t len)
{
    u32 sector = (u32)(offset / WII_SECTOR_SIZE);
    u32 sectorOffset = (u32)(offset % WII_SECTOR_SIZE);
    const u32 sectorsPerBuffer = READ_BUFFER_SIZE / WII_SECTOR_SIZE;

    if (len > (size_t)(READ_BUFFER_SIZE - sectorOffset))
        len = (size_t)(READ_BUFFER_SIZE - sectorOffset);

    if (m->rawCacheSectors && sector >= m->rawCacheStartSector &&
        sector < m->rawCacheStartSector + m->rawCacheSectors)
    {
        memcpy(ptr, m->rawBuffer + (sector - m->rawCacheStartSector) * WII_SECTOR_SIZE + sectorOffset, len);
        return (int)len;
    }

    if (!m->disc->readSectors(sector, sectorsPerBuffer, m->rawBuffer))
    {
        m->rawCacheSectors = 0;
        return -1;
    }
    m->rawCacheStartSector = sector;
    m->rawCacheSectors = sectorsPerBuffer;
    memcpy(ptr, m->rawBuffer + sectorOffset, len);
    return (int)len;
}

/* Loops ReadRaw() until len bytes are copied or a read fails. */
int ReadAbs(DFST_Mount* m, void* ptr, u64 offset, size_t len)
{
    size_t done = 0;
    char* dst = (char*)ptr;
    while (done < len)
    {
        int ret = ReadRaw(m, dst + done, offset + done, len - done);
        if (ret > 0)
            done += (size_t)ret;
        else if (ret == 0)
            break;
        else
            return -1;
    }
    return (int)done;
}

/* Single cache-fill read from WITHIN the partition DI_OpenPartition() left
 * open, via DI_Read() -- the higher-level call IOS auto-decrypts and
 * de-hashes transparently (see the file header comment). logicalOffset
 * here is a plain byte position in that clean stream -- no hash-cluster
 * math needed, unlike the raw ReadRaw() above.
 *
 * DI_Read()'s own offset parameter is Nintendo's usual disc-offset
 * convention (the byte offset shifted right by 2), and every buffer
 * passed to it must be 32-byte aligned -- both the cache buffer and the
 * chunk boundaries here are kept 32-byte aligned to respect that. */
int ReadPartitionRaw(DFST_Mount* m, void* ptr, u64 logicalOffset, size_t len)
{
    u64 alignedStart = logicalOffset & ~(u64)31;
    u32 within = (u32)(logicalOffset - alignedStart);

    if (m->partCacheLen && alignedStart >= m->partCacheStart &&
        alignedStart + within < m->partCacheStart + m->partCacheLen)
    {
        u64 within2 = logicalOffset - m->partCacheStart;
        size_t avail = (size_t)(m->partCacheLen - within2);
        size_t chunk = (len < avail) ? len : avail;
        if (chunk > 0)
        {
            memcpy(ptr, m->partBuffer + within2, chunk);
            return (int)chunk;
        }
    }

    int diRet = DI_Read(m->partBuffer, READ_BUFFER_SIZE, (u32)(alignedStart >> 2));
    if (diRet != 0)
    {
        g_lastDiReadError = diRet;
        m->partCacheLen = 0;
        return -1;
    }
    m->partCacheStart = alignedStart;
    m->partCacheLen = READ_BUFFER_SIZE;

    size_t avail = READ_BUFFER_SIZE - within;
    size_t chunk = (len < avail) ? len : avail;
    memcpy(ptr, m->partBuffer + within, chunk);
    return (int)chunk;
}

/* Loops ReadPartitionRaw() until len bytes are copied or a read fails. */
int ReadPartition(DFST_Mount* m, void* ptr, u64 logicalOffset, size_t len)
{
    size_t done = 0;
    char* dst = (char*)ptr;
    while (done < len)
    {
        int ret = ReadPartitionRaw(m, dst + done, logicalOffset + done, len - done);
        if (ret > 0)
            done += (size_t)ret;
        else if (ret == 0)
            break;
        else
            return -1;
    }
    return (int)done;
}

/* Scans the 4 volume groups at PARTITION_TABLE_INFO_OFFSET for the first
 * PARTITION_TYPE_DATA partition and returns its RAW offset field exactly
 * as stored on disc (i.e. NOT multiplied by 4) -- this is the form
 * DI_OpenPartition() itself expects, matching Nintendo's on-disc
 * convention for this specific field. */
bool FindDataPartitionOffset(DFST_Mount* m, u32* outPartitionOffsetRaw)
{
    u8 vgTable[0x20];
    if (ReadAbs(m, vgTable, PARTITION_TABLE_INFO_OFFSET, sizeof(vgTable)) != (int)sizeof(vgTable))
        return false;

    for (int vg = 0; vg < 4; ++vg)
    {
        u32 count = Be32(vgTable + vg * 8);
        u32 tableOffset = Be32(vgTable + vg * 8 + 4) * 4;
        if (count == 0)
            continue;
        if (count > MAX_PARTITIONS_PER_GROUP)
            count = MAX_PARTITIONS_PER_GROUP;

        for (u32 i = 0; i < count; ++i)
        {
            u8 entry[8];
            if (ReadAbs(m, entry, (u64)tableOffset + (u64)i * 8, sizeof(entry)) != (int)sizeof(entry))
                return false;
            u32 partOffsetRaw = Be32(entry);
            u32 partType = Be32(entry + 4);
            if (partType == PARTITION_TYPE_DATA)
            {
                *outPartitionOffsetRaw = partOffsetRaw;
                return true;
            }
        }
    }
    return false;
}

/* Asks IOS to open the DATA partition and derive its title key -- see
 * the file header comment for exactly what this does and doesn't do.
 * Records the raw return code either way (0 == success) so the caller
 * can surface *why* this failed, since "will only work on original
 * discs" means a failure here is a real, expected possibility to plan
 * for, not just a bug to paper over. After this succeeds, logical offset
 * 0 (via ReadPartition) is boot.bin's first byte, already decrypted. */
bool OpenDataPartition(u32 partitionOffsetRaw)
{
    int ret = DI_OpenPartition(partitionOffsetRaw);
    g_openPartitionError = ret;
    return ret == 0;
}

/* Reads boot.bin's fst_offset/fst_size fields and pulls the whole FST
 * (entry table + name string table) into a single malloc'd buffer. */
bool LoadFST(DFST_Mount* m)
{
    u8 hdr[0x430];
    int readRet = ReadPartition(m, hdr, 0, sizeof(hdr));
    if (readRet != (int)sizeof(hdr))
    {
        g_loadFstDetail = DISCFST_DETAIL_READ_FAILED;
        g_lastMagicRead = (u32)readRet; /* repurposed: bytes actually got, for debugging */
        return false;
    }

    /* Sanity check: this is the same plain Wii disc magic every build
     * (retail or homebrew) has at this fixed spot once properly
     * decrypted. If DI_OpenPartition() reported success but this isn't
     * here, something is still off (e.g. actually-still-encrypted data
     * slipping through, or the offset convention assumed by
     * ReadPartition() being wrong) -- refuse to guess further rather
     * than parse garbage as if it were a valid FST. */
    u32 magic = Be32(hdr + 0x18);
    g_lastMagicRead = magic;
    if (magic != WII_DISC_MAGIC)
    {
        g_loadFstDetail = DISCFST_DETAIL_BAD_MAGIC;
        return false;
    }

    u32 fstOffset = Be32(hdr + 0x424) * 4;
    u32 fstSize   = Be32(hdr + 0x428) * 4;
    if (fstOffset == 0 || fstSize < FST_ENTRY_SIZE || fstSize > FST_MAX_SIZE)
    {
        g_loadFstDetail = DISCFST_DETAIL_BAD_FST_FIELDS;
        g_lastFstOffset = fstOffset;
        g_lastFstSize = fstSize;
        return false;
    }

    u8* fst = (u8*)malloc(fstSize);
    if (!fst)
        return false;
    if (ReadPartition(m, fst, fstOffset, fstSize) != (int)fstSize)
    {
        free(fst);
        return false;
    }

    u32 entryCount = Be32(fst + 8); /* root entry's "next_index" == total entries */
    if (entryCount == 0 || (u64)entryCount * FST_ENTRY_SIZE > fstSize)
    {
        free(fst);
        return false;
    }

    m->fst = fst;
    m->fstEntryCount = entryCount;
    m->fstSize = fstSize;
    return true;
}

/* Never trust a raw "next index" field from the disc as an unchecked
 * array bound: a single corrupt or misparsed entry must not be able to
 * walk the traversal past the actual entry table, which is what was
 * happening before this existed (an out-of-range index led to reading
 * unrelated bytes -- including string-table text -- as if they were a
 * binary FST entry, which is how a plausible-looking but garbage file
 * size like the ~3.5 GB Dolphin refused to copy could come out of this). */
u32 SafeNextIndex(DFST_Mount* m, u32 raw)
{
    return (raw <= m->fstEntryCount) ? raw : m->fstEntryCount;
}

/* Same idea for file sizes: nothing this project ships (a texture, a
 * sound) is remotely close to this cap. Treating an implausible size as
 * "this entry is corrupt" and failing the lookup is much safer than
 * hexpassing it on to a malloc()+fread() of that size. */
constexpr u32 kMaxPlausibleFileSize = 64u * 1024 * 1024;

const char* EntryName(DFST_Mount* m, const u8* entry)
{
    u32 nameOffset = Be24(entry + 1);
    u64 stringTableStart = (u64)m->fstEntryCount * FST_ENTRY_SIZE;
    u64 stringTableSize = (m->fstSize > stringTableStart) ? (m->fstSize - stringTableStart) : 0;
    if (nameOffset >= stringTableSize)
        return ""; /* out of range -- never dereference an unchecked offset */

    /* Also make sure the string is actually terminated before the end of
     * the buffer, rather than trusting strlen() to stop in bounds. */
    const char* name = (const char*)(m->fst + stringTableStart + nameOffset);
    const char* end = (const char*)(m->fst + m->fstSize);
    if (!memchr(name, '\0', end - name))
        return "";
    return name;
}

/* Resolves a "dir/subdir/file.ext"-style path (device prefix, if any, is
 * stripped) against the in-memory FST. Root ("" or "/") resolves to the
 * whole top-level range without needing a real entry 0 lookup. */
bool Lookup(DFST_Mount* m, const char* rawPath, DFST_Lookup* out)
{
    char buf[DFST_MAX_PATH];
    const char* colon = strchr(rawPath, ':');
    const char* src = colon ? colon + 1 : rawPath;

    size_t n = strlen(src);
    if (n >= sizeof(buf))
        n = sizeof(buf) - 1;
    memcpy(buf, src, n);
    buf[n] = '\0';

    while (n > 1 && buf[n - 1] == '/')
        buf[--n] = '\0';

    const char* path = buf;
    while (*path == '/')
        ++path;

    if (*path == '\0')
    {
        out->isDir = true;
        out->index = 0;
        out->childStart = 1;
        out->childEnd = m->fstEntryCount;
        return true;
    }

    u32 rangeStart = 1, rangeEnd = m->fstEntryCount;

    for (;;)
    {
        const char* slash = strchr(path, '/');
        size_t segLen = slash ? (size_t)(slash - path) : strlen(path);
        bool isLast = (slash == NULL);

        bool matched = false;
        u32 i = rangeStart;
        while (i < rangeEnd)
        {
            const u8* e = m->fst + (u64)i * FST_ENTRY_SIZE;
            bool isDir = (e[0] != 0);
            const char* name = EntryName(m, e);

            if (strlen(name) == segLen && strncasecmp(name, path, segLen) == 0)
            {
                matched = true;
                if (isLast)
                {
                    out->isDir = isDir;
                    out->index = i;
                    if (isDir)
                    {
                        out->childStart = i + 1;
                        out->childEnd = SafeNextIndex(m, Be32(e + 8));
                    }
                    else
                    {
                        u32 size = Be32(e + 8);
                        if (size > kMaxPlausibleFileSize)
                            return false; /* corrupt entry -- refuse rather than propagate */
                        out->fileOffset = Be32(e + 4) * 4; /* logical offset -- see ReadPartition() */
                        out->fileSize = size;
                    }
                    return true;
                }
                if (!isDir)
                    return false; /* path continues past a file component */
                rangeStart = i + 1;
                rangeEnd = SafeNextIndex(m, Be32(e + 8));
                break;
            }

            if (isDir)
            {
                u32 next = SafeNextIndex(m, Be32(e + 8));
                i = (next > i) ? next : (i + 1); /* guarantee forward progress even if corrupt */
            }
            else
            {
                ++i;
            }
        }

        if (!matched)
            return false;

        path = slash + 1;
    }
}

void FillStat(struct stat* st, bool isDir, u32 size)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = (isDir ? S_IFDIR : S_IFREG) | S_IRUSR | S_IRGRP | S_IROTH;
    st->st_nlink = 1;
    st->st_size = (off_t)size;
    st->st_blksize = WII_SECTOR_SIZE;
    st->st_blocks = (size + WII_SECTOR_SIZE - 1) / WII_SECTOR_SIZE;
}

const devoptab_t* g_devoptabTemplate; /* set once kDeviceOpTab below exists */

DFST_Mount* GetMountFromPath(const char* path)
{
    if (!path)
        return NULL;
    devoptab_t* devops = (devoptab_t*)GetDeviceOpTab(path);
    if (!devops || !g_devoptabTemplate || devops->open_r != g_devoptabTemplate->open_r)
        return NULL;
    return (DFST_Mount*)devops->deviceData;
}

int _DFST_open_r(struct _reent* r, void* fileStruct, const char* path, int /*flags*/, int /*mode*/)
{
    DFST_File* file = (DFST_File*)fileStruct;
    DFST_Mount* m = GetMountFromPath(path);
    if (!m) { r->_errno = ENODEV; return -1; }

    DFST_Lookup lu;
    if (!Lookup(m, path, &lu)) { r->_errno = ENOENT; return -1; }
    if (lu.isDir) { r->_errno = EISDIR; return -1; }

    file->fileOffset = lu.fileOffset;
    file->fileSize = lu.fileSize;
    file->cursor = 0;
    file->inUse = true;
    file->mount = m;
    return (int)file;
}

int _DFST_close_r(struct _reent* r, void* fd)
{
    DFST_File* file = (DFST_File*)fd;
    if (!file->inUse) { r->_errno = EBADF; return -1; }
    file->inUse = false;
    return 0;
}

ssize_t _DFST_read_r(struct _reent* r, void* fd, char* ptr, size_t len)
{
    DFST_File* file = (DFST_File*)fd;
    if (!file->inUse) { r->_errno = EBADF; return -1; }
    if (file->cursor >= file->fileSize)
        return 0;
    if (len > file->fileSize - file->cursor)
        len = file->fileSize - file->cursor;
    if (len == 0)
        return 0;

    int ret = ReadPartition(file->mount, ptr, (u64)file->fileOffset + file->cursor, len);
    if (ret < 0) { r->_errno = EIO; return -1; }
    file->cursor += (u32)ret;
    return ret;
}

off_t _DFST_seek_r(struct _reent* r, void* fd, off_t pos, int dir)
{
    DFST_File* file = (DFST_File*)fd;
    if (!file->inUse) { r->_errno = EBADF; return -1; }

    off_t position;
    switch (dir)
    {
        case SEEK_SET: position = pos; break;
        case SEEK_CUR: position = (off_t)file->cursor + pos; break;
        case SEEK_END: position = (off_t)file->fileSize + pos; break;
        default: r->_errno = EINVAL; return -1;
    }
    if (position < 0 || position > (off_t)file->fileSize) { r->_errno = EINVAL; return -1; }
    file->cursor = (u32)position;
    return position;
}

int _DFST_fstat_r(struct _reent* r, void* fd, struct stat* st)
{
    DFST_File* file = (DFST_File*)fd;
    if (!file->inUse) { r->_errno = EBADF; return -1; }
    FillStat(st, false, file->fileSize);
    return 0;
}

int _DFST_stat_r(struct _reent* r, const char* path, struct stat* st)
{
    DFST_Mount* m = GetMountFromPath(path);
    if (!m) { r->_errno = ENODEV; return -1; }
    DFST_Lookup lu;
    if (!Lookup(m, path, &lu)) { r->_errno = ENOENT; return -1; }
    FillStat(st, lu.isDir, lu.isDir ? 0 : lu.fileSize);
    return 0;
}

int _DFST_chdir_r(struct _reent* r, const char* path)
{
    DFST_Mount* m = GetMountFromPath(path);
    if (!m) { r->_errno = ENODEV; return -1; }
    DFST_Lookup lu;
    if (!Lookup(m, path, &lu)) { r->_errno = ENOENT; return -1; }
    if (!lu.isDir) { r->_errno = ENOTDIR; return -1; }
    return 0;
}

DIR_ITER* _DFST_diropen_r(struct _reent* r, DIR_ITER* dirState, const char* path)
{
    DFST_DirState* state = (DFST_DirState*)(dirState->dirStruct);
    DFST_Mount* m = GetMountFromPath(path);
    if (!m) { r->_errno = ENODEV; return NULL; }

    DFST_Lookup lu;
    if (!Lookup(m, path, &lu)) { r->_errno = ENOENT; return NULL; }
    if (!lu.isDir) { r->_errno = ENOTDIR; return NULL; }

    state->index = lu.childStart;
    state->rangeEnd = lu.childEnd;
    state->mount = m;
    state->inUse = true;
    return dirState;
}

int _DFST_dirreset_r(struct _reent* r, DIR_ITER* dirState)
{
    /* Re-resolving the range needs the original path, which isn't kept
     * around here -- this codebase never calls rewinddir(), so this just
     * fails cleanly instead of silently doing nothing. */
    DFST_DirState* state = (DFST_DirState*)(dirState->dirStruct);
    if (!state->inUse) { r->_errno = EBADF; return -1; }
    r->_errno = ENOSYS;
    return -1;
}

int _DFST_dirnext_r(struct _reent* r, DIR_ITER* dirState, char* filename, struct stat* st)
{
    DFST_DirState* state = (DFST_DirState*)(dirState->dirStruct);
    if (!state->inUse) { r->_errno = EBADF; return -1; }
    if (state->index >= state->rangeEnd) { r->_errno = ENOENT; return -1; }

    DFST_Mount* m = state->mount;
    u32 i = state->index;
    const u8* e = m->fst + (u64)i * FST_ENTRY_SIZE;
    bool isDir = (e[0] != 0);
    const char* name = EntryName(m, e);

    size_t nameLen = strlen(name);
    if (nameLen > DFST_MAX_NAME)
        nameLen = DFST_MAX_NAME;
    memcpy(filename, name, nameLen);
    filename[nameLen] = '\0';

    u32 rawSize = Be32(e + 8);
    u32 size = (!isDir && rawSize > kMaxPlausibleFileSize) ? 0 : rawSize;
    FillStat(st, isDir, isDir ? 0 : size);

    if (isDir)
    {
        u32 next = SafeNextIndex(m, rawSize);
        state->index = (next > i) ? next : (i + 1);
    }
    else
    {
        state->index = i + 1;
    }
    return 0;
}

int _DFST_dirclose_r(struct _reent* r, DIR_ITER* dirState)
{
    DFST_DirState* state = (DFST_DirState*)(dirState->dirStruct);
    if (!state->inUse) { r->_errno = EBADF; return -1; }
    state->inUse = false;
    return 0;
}

const devoptab_t kDeviceOpTab =
{
    NULL,                       /* name -- filled in per-mount */
    sizeof(DFST_File),
    _DFST_open_r,
    _DFST_close_r,
    NULL,                       /* write_r  -- read-only fs */
    _DFST_read_r,
    _DFST_seek_r,
    _DFST_fstat_r,
    _DFST_stat_r,
    NULL,                       /* link_r */
    NULL,                       /* unlink_r */
    _DFST_chdir_r,
    NULL,                       /* rename_r */
    NULL,                       /* mkdir_r */
    sizeof(DFST_DirState),
    _DFST_diropen_r,
    _DFST_dirreset_r,
    _DFST_dirnext_r,
    _DFST_dirclose_r,
    NULL,                       /* statvfs_r -- unused by this codebase */
    NULL,                       /* ftruncate_r */
    NULL,                       /* fsync_r */
    NULL                        /* deviceData -- filled in per-mount */
};

} // namespace

extern "C" bool DiscFST_Mount(void)
{
    static const char kName[] = "wiidisc";
    char devname[16];

    g_devoptabTemplate = &kDeviceOpTab;
    g_status = DISCFST_NOT_ATTEMPTED;
    g_openPartitionError = 0;

    /* Nothing else in this codebase calls this, and __io_wiidvd's own
     * startup() silently does nothing useful without it having run
     * first -- this was missing entirely before and would have made
     * every attempt fail right here, regardless of anything else. */
    DI_Init();

    if (!__io_wiidvd.startup() || !__io_wiidvd.isInserted())
    {
        g_status = DISCFST_NO_DISC;
        return false;
    }

    snprintf(devname, sizeof(devname), "%s:", kName);
    if (FindDevice(devname) >= 0)
        return false; /* already mounted */

    /* IOS's IPC mechanism requires buffers it DMAs into/out of to be
     * 32-byte aligned in absolute memory -- a hard requirement, not just
     * a performance nicety. Plain malloc() on this toolchain doesn't
     * guarantee that for the whole struct (only for individual fields
     * relative to wherever the struct itself lands), so this could
     * plausibly be exactly why DI_Read() was failing outright. */
    DFST_Mount* m = (DFST_Mount*)memalign(32, sizeof(DFST_Mount));
    if (!m)
        return false;
    memset(m, 0, sizeof(*m));
    m->disc = &__io_wiidvd;

    u32 partitionOffsetRaw;
    if (!FindDataPartitionOffset(m, &partitionOffsetRaw))
    {
        g_status = DISCFST_NO_DATA_PARTITION;
        free(m);
        return false;
    }

    if (!OpenDataPartition(partitionOffsetRaw))
    {
        g_status = DISCFST_OPEN_PARTITION_FAILED; /* g_openPartitionError already set */
        free(m);
        return false;
    }

    if (!LoadFST(m))
    {
        /* Distinguish "opened fine but the magic wasn't there" from "FST
         * fields themselves looked wrong" isn't critical here -- both are
         * surfaced together, since either means something to check by
         * hand rather than a specific code path to add. */
        g_status = DISCFST_BAD_FST;
        DI_ClosePartition();
        free(m);
        return false;
    }

    devoptab_t* devops = (devoptab_t*)malloc(sizeof(kDeviceOpTab) + sizeof(kName));
    if (!devops)
    {
        free(m->fst);
        DI_ClosePartition();
        free(m);
        return false;
    }
    char* nameCopy = (char*)(devops + 1);
    memcpy(devops, &kDeviceOpTab, sizeof(kDeviceOpTab));
    strcpy(nameCopy, kName);
    devops->name = nameCopy;
    devops->deviceData = m;

    if (AddDevice(devops) < 0)
    {
        free(m->fst);
        DI_ClosePartition();
        free(m);
        free(devops);
        return false;
    }
    g_status = DISCFST_MOUNTED;
    return true;
}

extern "C" DiscFST_Status DiscFST_GetStatus(void)
{
    return g_status;
}

extern "C" int DiscFST_GetOpenPartitionError(void)
{
    return g_openPartitionError;
}

extern "C" int DiscFST_GetBadFstDetail(void)
{
    return (int)g_loadFstDetail;
}

extern "C" unsigned int DiscFST_GetLastMagic(void)
{
    return g_lastMagicRead;
}

extern "C" unsigned int DiscFST_GetLastFstOffset(void)
{
    return g_lastFstOffset;
}

extern "C" unsigned int DiscFST_GetLastFstSize(void)
{
    return g_lastFstSize;
}

extern "C" int DiscFST_GetLastDiReadError(void)
{
    return g_lastDiReadError;
}
