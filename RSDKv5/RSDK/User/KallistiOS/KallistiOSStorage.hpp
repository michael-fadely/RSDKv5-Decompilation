#if RETRO_PLATFORM == RETRO_KALLISTIOS
using namespace RSDK;
#include <kos.h>
#include <errno.h>
#include "lzfx.h"

#define VMU_DEBUG 0

extern "C" {
    extern mutex_t io_lock;
}

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
            printf("LoadUserFileFromVMU: cannot open %s for rdonly (%s)\n", fn, strerror(errno));
#endif
            return false;
        } else {
            if (compressed) {
#if VMU_DEBUG
                vid_border_color(0x00, 0xFF, 0x00);
#endif
                fs_seek(vmu_file, 512 + sizeof(vmu_hdr_t), SEEK_SET);

                ssize_t readrv = fs_read(vmu_file, &actual_size, 4);
                if (readrv != 4) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("LoadUserFileFromVMU: expected 4 but read %d\n", rv);
#endif
                    fs_close(vmu_file);
                    vmu_file = FILEHND_INVALID;

                    return false;
                }

#if VMU_DEBUG
                printf("LoadUserFileFromVMU: got size of %d from size word\n", actual_size);
#endif

                readrv = fs_read(vmu_file, saveOutbuf, actual_size);
                if (readrv != actual_size) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("LoadUserFileFromVMU: expected %d but read %d\n", actual_size, rv);
#endif
                    return false;
                }

#if VMU_DEBUG
                if (actual_size > 65536) {
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("clearly invalid actual_size %d\n", actual_size);
                    fs_close(vmu_file);
                    vmu_file = FILEHND_INVALID;
                    return false;
                }
#endif

                unsigned int uncompressed_save_size = actual_size;
#if VMU_DEBUG
                vid_border_color(0x00, 0xFF, 0xFF);
#endif
                int derv = lzfx_decompress(saveOutbuf, actual_size, outbuf, &uncompressed_save_size);
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
                fs_seek(vmu_file, 512+ sizeof(vmu_hdr_t), SEEK_SET);
                ssize_t rv = fs_read(vmu_file, outbuf, outsize);
                if (rv != (ssize_t)outsize) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
#endif
                    fs_close(vmu_file);
                    vmu_file = FILEHND_INVALID;
                    return false;
                }
            }
        }

        fs_close(vmu_file);
        vmu_file = FILEHND_INVALID;
        return b32retval;
    }

static bool32 SaveUserFileToVMU(const char *filename, void *outbuf, uint32 outsize) {
    mutex_lock_scoped(&io_lock);
    char *fn = GetVMUFilename(filename);
    int isSave = 0;
    int isOther = 0;
    int needCompressed = outsize > 1024;
    file_t vmu_file = FILEHND_INVALID;

    if (strstr(filename, "SaveData")) {
        isSave = 1;
    } else if(!strstr(filename, "Achieve")) {
        isOther = 1;
    }

#if VMU_DEBUG
    vid_border_color(0xFF, 0xFF, 0x00);
#endif

    vmu_file = fs_open(fn, O_RDWR | O_CREAT | O_TRUNC | O_META);
    if (FILEHND_INVALID == vmu_file) {
#if VMU_DEBUG
        vid_border_color(0xFF, 0x00, 0x00);
        printf("SaveUserFileToVMU: cannot open %s for rdwr|creat|trunc (%s)\n", fn, strerror(errno));
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
    pkg.icon_data = (uint8*)0x8C010000;//outbuf;
    for (int i=0;i<16;i++) {
        pkg.icon_pal[i] = 0x8000 | (i << 11) | (i << 6) | (i << 1);
    }
    pkg.icon_anim_speed = 0;
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
    fs_close(vmu_file);
    free(pkg_out);
    if (writerv != pkg_size) {
#if VMU_DEBUG
        vid_border_color(0x00, 0xFF, 0x00);
        printf("wrote %d but expected %d\n", writerv, pkg_size);
#endif
        return false;
    }
#if VMU_DEBUG
    vid_border_color(0xFF, 0x00, 0xFF);
#endif
    return true;
}
};
#endif