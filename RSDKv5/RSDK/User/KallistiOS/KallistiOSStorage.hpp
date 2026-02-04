#if RETRO_PLATFORM == RETRO_KALLISTIOS

using namespace RSDK;

#include <kos.h>
#include <errno.h>
// utilizing LZFX codec for save data compression
#include "lzfx.h"

#define VMU_DEBUG 0

static char icon_fn[256];
static uint8 icondata[512];
static char vmu_userfn[256];

struct KallistiOSUserStorage : RSDK::SKU::UserStorage {
    // gives a filename oriented to the first attached VMU, regardless of the controller it is in
    // gives default /vmu/a1/ path if VMU not found
    // that ends up getting caught as "file not found" in other functions
    static char *GetVMUFilename(const char *filename) {
        maple_device_t *vmudev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
        int port = 0;
        int unit = 1;
        if (vmudev) {
            port = vmudev->port;
            unit = vmudev->unit;
        }

        if (strstr(filename, "SaveData")) // SaveData.bin -> mania.sav
            sprintf(vmu_userfn, "/vmu/%c%d/mania.sav", 'a'+port, unit);
        else if(strstr(filename, "Achieve")) // Achievements.bin -> mania.ach
            sprintf(vmu_userfn, "/vmu/%c%d/mania.ach", 'a'+port, unit);
        else // anything else -> first 8 chars suffixed with .man - this shouldn't happen but handle it anyway
            sprintf(vmu_userfn, "/vmu/%c%d/%.8s.man", 'a'+port, unit, filename);

        return vmu_userfn;
    }

    // delete a file from the VMU filesystem
    static bool32 DeleteUserFileFromVMU(const char *filename) {
        char *fn = GetVMUFilename(filename);

        // do not perform unlink except for SaveData.bin or Achievements.bin
        if ((!strstr(filename, "SaveData")) && (!strstr(filename, "Achieve"))) {
#if VMU_DEBUG
            printf("Was directed to remove %s, will not comply.\n", filename);
#endif
            return false;
        }
        int rv = fs_unlink(fn);
        return (rv == 0);
    }

    // load SaveData or Achievements from VMU
    static bool32 LoadUserFileFromVMU(const char *filename, void *outbuf, uint32 outsize) {
        bool32 retval = true;
        file_t vmu_file = FILEHND_INVALID;
        char *fn = GetVMUFilename(filename);
        int compressed = outsize > 1024;
        uint32 allocSize = outsize + 32768;

        // need some temporary storage for the VMU data
        // leave it to be reclaimed by RSDK 😬
        uint8 *saveOutbuf;
        AllocateStorage((void **)&saveOutbuf, allocSize, DATASET_TMP, true);
        if (saveOutbuf == nullptr) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("LoadUserFileFromVMU: could not allocate temp storage\n");
#endif
            return false;
        }
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

            int closerv = fs_close(vmu_file);
            if (closerv == -1) {
        #if VMU_DEBUG
                vid_border_color(0xFF, 0x00, 0x00);
                printf("LoadUserFileFromVMU: close %s failed (%s)\n", fn, strerror(errno));
        #endif
            }

            // data was stored through use of a `vmu_pkg`
            // load it the same way
            vmu_pkg_t pkg {};
            if(vmu_pkg_parse(saveOutbuf, vmu_size, &pkg) < 0) {
#if VMU_DEBUG
                vid_border_color(0xFF, 0x00, 0x00);
                printf("LoadUserFileFromVMU: failed to parse vmu package\n");
#endif
                return false;
            }

            // SaveData.bin is 128 blocks uncompressed, it goes through the decompression path
            if (compressed) {
#if VMU_DEBUG
                vid_border_color(0x00, 0xFF, 0x00);
#endif
                // first 4 bytes of package data are the actual size of the compressed data
                actual_size = *(uint32_t *)pkg.data;
                // immediately followed by the compressed data
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
                // error handling paths follow
#if VMU_DEBUG
                if (derv < 0 && derv != LZFX_ESIZE3) {
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("decompress get size failure %d\n", derv);
                    retval = false;
                }
                else if (derv < 0) {
#else
                if (derv < 0) {
#endif
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("decompress failure %d\n", derv);
#endif
                    retval = false;
                } else if (derv > 0) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("decompress failure %d\n", derv);
#endif
                    retval = false;
                }

                if (0 == uncompressed_save_size) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("failed to decompress save data\n");
#endif
                    retval = false;
                }

                if (allocSize < uncompressed_save_size) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("uncompressed save too large %d < %d\n", allocSize, uncompressed_save_size);
#endif
                    retval = false;
                }

                if ((uncompressed_save_size) != outsize) {
#if VMU_DEBUG
                    vid_border_color(0xFF, 0x00, 0x00);
                    printf("wrong decompress size (unc %d out %d)\n", uncompressed_save_size, outsize);
#endif
                    retval = false;
                }
            } else {
                // Achievements.bin is only 2 blocks uncompressed so we store and load it without compression
#if VMU_DEBUG
                vid_border_color(0x00, 0x00, 0xFF);
#endif
                memcpy(outbuf, pkg.data, outsize);
            }
        }
        return retval;
    }

static bool32 SaveUserFileToVMU(const char *filename, void *outbuf, uint32 outsize) {
    char *fn = GetVMUFilename(filename);
    int exists = 0;
    int isSave = 0;
    int isAch = 0;
    // large files would be compressed
    int needCompressed = (outsize > 1024u);
    file_t vmu_file = FILEHND_INVALID;

    // obvious, I hope
    if (strstr(filename, "SaveData")) {
        isSave = 1;
    } else if(strstr(filename, "Achieve")) {
        isAch = 1;
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
    vmu_pkg_t pkg {};
    if (isSave)
        strcpy(pkg.desc_long, "Saved Games");
    else if (isAch)
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
        // save data is 64k uncompressed
        // hopefully 64k is enough for compressed plus a wasted int to hold the compressed size -_-
        // we need a bit of extra space to keep the codec from failing for reasons
        unsigned int compressed_size = outsize + 32768;
        // need some temporary storage for the VMU data
        // leave it to be reclaimed by RSDK 😬
        uint8 *saveOutbuf;
        AllocateStorage((void **)&saveOutbuf, compressed_size, DATASET_TMP, true);
        if (saveOutbuf == nullptr) {
#if VMU_DEBUG
            vid_border_color(0xFF, 0x00, 0x00);
            printf("could not tmp alloc for save\n");
#endif
            return false;
        }
        // do the compression before generating `vmu_pkg`
        // necessary to know the amount of data to be written
        // note that the output buffer given for the compressed data is 4 bytes after the start of our temp allocation
        // leaves the first int worth of space for the actual compressed data size
        int rv = lzfx_compress(outbuf, outsize, saveOutbuf + 4, &compressed_size);
        // error handling paths
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
        // first 4 bytes of output data are an int storing the actual compressed data size
        *(uint32*)saveOutbuf = compressed_size;
        // set the payload-related fields in the package
        pkg.data_len = compressed_size + 4;
        pkg.data = (const uint8 *)saveOutbuf;
    } else {
        // set the payload-related fields in the package
        pkg.data_len = outsize;
        pkg.data = (const uint8 *)outbuf;
    }

    // now actually generate the vmu_pkg
    // and hope it doesn't fail due to a lack of free memory
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

#if VMU_DEBUG
    vid_border_color(0xFF, 0x00, 0xFF);
#endif

    int closerv = fs_close(vmu_file);
    if (closerv == -1) {
#if VMU_DEBUG
        vid_border_color(0xFF, 0x00, 0x00);
        printf("SaveUserFileToVMU: close %s failed (%s)\n", fn, strerror(errno));
#endif
    }

    free(pkg_out);
    return true;
}
};

#endif