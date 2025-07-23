#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "mz.h"
#include "mz_zip.h"

#include "my_zip_archive.h"

static int32_t copy_raw(void *source, void *target)
{
    mz_zip_file *file_info = NULL;
    int cleanup_ret = MZ_OK;

    int ret = mz_zip_entry_get_info(source, &file_info);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to get inner file metadata: %d\n", ret);
        goto done;
    }

    /* Open raw file in input zip */
    ret = mz_zip_entry_read_open(source, 1, NULL);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open inner file: %d\n", ret);
        goto done;
    }

    mz_zip_file target_file_info = *file_info;

    /* Open raw file in output zip */
    ret = mz_zip_entry_write_open(target, &target_file_info, 0, 1, NULL);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open inner file: %d\n", ret);
        goto close_source_entry;
    }

    char buf[UINT16_MAX] = { 0 };
    int read = 0, written = 0;

    while ((read = mz_zip_entry_read(source, buf, sizeof(buf))) > 0) {
        written = mz_zip_entry_write(target, buf, read);
        if (written != read) {
            fprintf(stderr, "Failed to write data to inner file\n");
            ret = MZ_STREAM_ERROR;
            goto close_target_entry;
        }
    }

    if (read != 0) {
        fprintf(stderr, "Failed to read inner file\n");
        ret = MZ_STREAM_ERROR;
        goto close_target_entry;
    }

    ret = RET_OK;

close_target_entry:
    cleanup_ret = mz_zip_entry_close_raw(target, file_info->uncompressed_size,
                                         file_info->crc);
    if (cleanup_ret != MZ_OK) {
        fprintf(stderr, "Failed to close inner file: %d\n", cleanup_ret);
        if (ret == RET_OK) {
            ret = cleanup_ret;
        }
    }

close_source_entry:
    cleanup_ret = mz_zip_entry_close(source);
    if (cleanup_ret != MZ_OK) {
        fprintf(stderr, "Failed to close inner file: %d\n", cleanup_ret);
        if (ret == RET_OK) ret = cleanup_ret;
    }

done:
    if (ret != RET_OK) {
        ret = RET_FAIL;
    }

    return ret;
}

static int32_t copy_all_raw(void *source, void *target)
{
    int ret = mz_zip_goto_first_entry(source);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to go to first entry: %d\n", ret);
        return ret;
    }

    do {
        ret = copy_raw(source, target);
        if (ret != RET_OK) {
            return ret;
        }
    } while ((ret = mz_zip_goto_next_entry(source)) == MZ_OK);

    if (ret != MZ_END_OF_LIST) {
        fprintf(stderr, "Failed to go to next entry: %d\n", ret);
        return ret;
    }

    return RET_OK;
}

int main(int argc, char *argv[])
{
    int8_t archive_mode     = MODE_NORMAL;
    int ret                 = MZ_OK;
    int exit_code           = EXIT_SUCCESS;

    my_zip_archive source_archive = { 0 };
    my_zip_archive target_archive = { 0 };

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source file> <target file> [--normal|--memory|--bufstream]\n", argv[0]);
        goto done;
    }

    if (argc == 4) {
        if (!strcmp(argv[3], "--normal")) {
            archive_mode = MODE_NORMAL;
        }
        else if (!strcmp(argv[3], "--bufstream")) {
            archive_mode = MODE_BUFSTREAM;
        }
        else if (!strcmp(argv[3], "--memory")) {
            archive_mode = MODE_MEMORY;
        }
        else {
            fprintf(stderr, "Invalid parameter.\n");
            fprintf(stderr, "Usage: %s <source file> <target file> [--normal|--memory|--bufstream]\n", argv[0]);
            goto done;
        }
    }

    source_archive.file_path = argv[1];
    target_archive.file_path = argv[2];

    ret = open_archive(&source_archive, archive_mode, MZ_OPEN_MODE_READ);
    if (ret != RET_OK) {
        goto done;
    }

    ret = open_archive(&target_archive, archive_mode, MZ_OPEN_MODE_READWRITE | MZ_OPEN_MODE_CREATE);
    if (ret != RET_OK) {
        goto done;
    }

    ret = copy_all_raw(source_archive.mz_handle, target_archive.mz_handle);

done:
    if (ret != RET_OK) {
        exit_code = EXIT_FAILURE;
    }

    ret = close_archive(&source_archive);
    if (ret != RET_OK) {
        exit_code = EXIT_FAILURE;
    }

    ret = close_archive(&target_archive);
    if (ret != RET_OK) {
        exit_code = EXIT_FAILURE;
    }

    return exit_code;
}
