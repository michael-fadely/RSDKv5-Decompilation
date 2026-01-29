#if RETRO_PLATFORM == RETRO_KALLISTIOS

using namespace RSDK;

#include <kos.h>
#include <errno.h>
#include "lzfx.h"

#define VMU_DEBUG 0

extern "C" {
    extern mutex_t io_lock;
}

static char icon_fn[256];
static uint8 icondata[512];
static char vmu_userfn[256];

struct KallistiOSUserStorage : RSDK::SKU::UserStorage {
    static char *GetVMUFilename(const char *filename) {
        // TODO logic to find *first* attached vmu and store *there*
        if (strstr(filename, "SaveData")) {
            sprintf(vmu_userfn, "/vmu/a1/maniasav.rec");
        } else if(strstr(filename, "Achieve")) {
            sprintf(vmu_userfn, "/vmu/a1/maniaach.rec");
        } else {
            sprintf(vmu_userfn, "/vmu/a1/%.8s.rec", filename);
        }
        return vmu_userfn;
    }

    static bool32 DeleteUserFileFromVMU(const char *filename) {
        mutex_lock_scoped(&io_lock);
        char *fn = GetVMUFilename(filename);

        if ((!strstr(filename, "SaveData")) && (!strstr(filename, "Achieve"))) {
#if VMU_DEBUG
            printf("Was directed to remove %s, will not comply.\n", filename);
#endif
            return false;
        }
        int rv = fs_unlink(fn);
        return (rv == 0);
    }

    static bool32 LoadUserFileFromVMU(const char *filename, void *outbuf, uint32 outsize) {
        mutex_lock_scoped(&io_lock);
        file_t vmu_file = FILEHND_INVALID;
        char *fn = GetVMUFilename(filename);
        uint8 *saveOutbuf;
        uint32 allocSize;
        int compressed = outsize > 1024;
        allocSize = outsize + 32768;
        AllocateStorage((void **)&saveOutbuf, allocSize, DATASET_TMP, false);
        if (saveOutbuf == nullptr) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("LoadUserFileFromVMU: could not allocate temp storage\n");
#endif
            return false;
        }
        memset(saveOutbuf, 0, allocSize);
        bool32 b32retval = true;
        uint32 actual_size = 0;
        vmu_file = fs_open(fn, O_RDONLY | O_META);
        if (FILEHND_INVALID == vmu_file) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("LoadUserFileFromVMU: cannot open %s for reading (%s)\n", fn, strerror(errno));
#endif
            return false;
        } else {
            off_t vmu_size = fs_seek(vmu_file, 0, SEEK_END);
            fs_seek(vmu_file, 0, SEEK_SET);
            ssize_t read1rv = fs_read(vmu_file, saveOutbuf, vmu_size);
            if (read1rv != vmu_size) {
#if VMU_DEBUG
                vid_border_color(0xFF, 0x00, 0x00);
                printf("LoadUserFileFromVMU: read %d requested %d\n", read1rv, vmu_size);
#endif
                fs_close(vmu_file);
                return false;
            }
            fs_close(vmu_file);

            vmu_pkg_t pkg;
            memset(&pkg, 0, sizeof(pkg));
            if(vmu_pkg_parse(saveOutbuf, vmu_size, &pkg) < 0) {
#if VMU_DEBUG
                vid_border_color(0xFF, 0x00, 0x00);
                printf("LoadUserFileFromVMU: failed to parse vmu package\n");
#endif
                return false;
            }

            if (compressed) {
#if VMU_DEBUG
                vid_border_color(0x00, 0xFF, 0x00);
#endif

                actual_size = *(uint32_t *)pkg.data;
                uint8_t *compData = (uint8_t *)(pkg.data + 4);

#if VMU_DEBUG
                printf("LoadUserFileFromVMU: got size of %d from size word\n", actual_size);

                if (actual_size > 65536) {
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("clearly invalid actual_size %d\n", actual_size);
                    fs_close(vmu_file);
                    return false;
                }
#endif

                unsigned int uncompressed_save_size = allocSize;
#if VMU_DEBUG
                vid_border_color(0x00, 0xFF, 0xFF);
#endif
                int derv = lzfx_decompress(compData, actual_size, outbuf, &uncompressed_save_size);
#if VMU_DEBUG
                if (derv < 0 && derv != LZFX_ESIZE3) {
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("decompress get size failure %d\n", derv);
                    b32retval = false;
                }
                else if (derv < 0) {
#else
                if (derv < 0) {
#endif
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("decompress failure %d\n", derv);
#endif
                    b32retval = false;
                } else if (derv > 0) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("decompress failure %d\n", derv);
#endif
                    b32retval = false;
                }

                if (0 == uncompressed_save_size) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("failed to decompress save data\n");
#endif
                    b32retval = false;
                }

                if (allocSize < uncompressed_save_size) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("uncompressed save too large %d < %d\n", allocSize, uncompressed_save_size);
#endif
                    b32retval = false;
                }

                if ((uncompressed_save_size) != outsize) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("wrong decompress size (unc %d out %d)\n", uncompressed_save_size, outsize);
#endif
                    b32retval = false;
                }
            } else {
#if VMU_DEBUG
                vid_border_color(0x00, 0x00, 0xFF);
#endif
                memcpy(outbuf, pkg.data, outsize);
            }
        }
#if 0
        int closerv = fs_close(vmu_file);
        if (closerv == -1) {
            printf("LoadUserFileToVMU: close %s failed (%s)\n", fn, strerror(errno));
            b32retval = false;
        }
#endif
        return b32retval;
    }

static bool32 SaveUserFileToVMU(const char *filename, void *outbuf, uint32 outsize) {
    mutex_lock_scoped(&io_lock);
    char *fn = GetVMUFilename(filename);
    int exists = 0;
    int isSave = 0;
    int isOther = 0;
    int needCompressed = (outsize > 1024u);
    file_t vmu_file = FILEHND_INVALID;

    if (strstr(filename, "SaveData")) {
        isSave = 1;
    } else if(!strstr(filename, "Achieve")) {
        isOther = 1;
    }

#if VMU_DEBUG
    vid_border_color(0xFF, 0xFF, 0x00);
#endif
    vmu_file = fs_open(fn, O_RDONLY | O_META);
    if (FILEHND_INVALID != vmu_file) {
        exists = 1;
        fs_close(vmu_file);
    }

    if (!exists)
        vmu_file = fs_open(fn, O_WRONLY | O_CREAT | O_META);
    else
        vmu_file = fs_open(fn, O_WRONLY | O_META);

    if (FILEHND_INVALID == vmu_file) {
#if VMU_DEBUG
        vid_border_color(0xFF, 0x00, 0x00);
        printf("SaveUserFileToVMU: cannot open %s for writing (%s)\n", fn, strerror(errno));
#endif
        return false;
    }
    vmu_pkg_t pkg;
    memset(&pkg, 0, sizeof(vmu_pkg_t));
    if (isSave)
        strcpy(pkg.desc_long, "Saved Games");
    else if (!isOther)
        strcpy(pkg.desc_long, "Achievements");
    else
        strncpy(pkg.desc_long, filename, MIN(strlen(filename), 8));

    strcpy(pkg.desc_short, "Sonic Mania");
    strcpy(pkg.app_id, "Sonic Mania");
    pkg.icon_cnt = 1;
    pkg.icon_data = icondata;
    pkg.icon_anim_speed = 5;
    if (isSave)
        sprintf(icon_fn, "%s/sonic.ico", KOS_USER_DIR);
    else
        sprintf(icon_fn, "%s/mighty.ico", KOS_USER_DIR);
    vmu_pkg_load_icon(&pkg, icon_fn);
    if (needCompressed) {
        uint8 *saveOutbuf;
        unsigned int compressed_size = outsize + 32768;

        // save data is 64k uncompressed
        // hopefully 64k is enough for compressed plus a wasted to hold the compressed size -_-
        AllocateStorage((void **)&saveOutbuf, outsize + 32768, DATASET_TMP, false);
        if (saveOutbuf == nullptr) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("could not tmp alloc for save\n");
#endif
            return false;
        }
        memset(saveOutbuf, 0, outsize);
        int rv = lzfx_compress(outbuf, outsize, saveOutbuf + 4, &compressed_size);
        if (0 > rv) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("lzfx_compress failed %d\n", rv);
#endif
            return false;
        }

        if (compressed_size > (65536-512)) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("compressed save too large\n");
#endif
            return false;
        }

        *(uint32*)saveOutbuf = compressed_size;

        pkg.data_len = compressed_size + 4;
        pkg.data = (const uint8 *)saveOutbuf;
    } else {
        pkg.data_len = outsize;
        pkg.data = (const uint8 *)outbuf;
    }

    uint8_t *pkg_out;
    ssize_t pkg_size;
    int pbrv = vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
    if ((pbrv < 0) || (!pkg_out) || (pkg_size <= 0)) {
#if VMU_DEBUG
        vid_border_color(0xFF, 0x00, 0x00);
        printf("SaveUserFileToVMU: vmu_pkg_build failed %d %08x %d\n", pbrv, pkg_out, pkg_size);
#endif
        fs_close(vmu_file);
        vmu_file = FILEHND_INVALID;
        return false;
    }

#if VMU_DEBUG
    vid_border_color(0x00, 0xFF, 0x00);
#endif
    fs_seek(vmu_file, 0, SEEK_SET);
    ssize_t writerv = fs_write(vmu_file, pkg_out, pkg_size);
    if (writerv != pkg_size) {
#if VMU_DEBUG
        vid_border_color(0xFF, 0x00, 0x00);
        printf("wrote %d but expected %d\n", writerv, pkg_size);
#endif
        fs_close(vmu_file);
        return false;
    }

    int closerv = fs_close(vmu_file);
    if (closerv == -1) {
#if VMU_DEBUG
        vid_border_color(0xFF, 0x00, 0x00);
        printf("SaveUserFileToVMU: close %s failed (%s)\n", fn, strerror(errno));
#endif
    }

    free(pkg_out);
#if VMU_DEBUG
    vid_border_color(0xFF, 0x00, 0xFF);
#endif
    return true;
}
};

#endif