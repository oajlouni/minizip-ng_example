#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "mz.h"
#include "mz_strm_buf.h"
#include "mz_strm_os.h"
#include "mz_strm_mem.h"
#include "mz_zip.h"
#include "mz_strm.h"

#include "my_zip_archive.h"

static int32_t read_file_into_buf(const char *file_path, uint8_t **buf, int32_t *buf_size)
{
    int32_t ret = RET_OK;

    FILE *fd = fopen(file_path, "rb");
    if (fd == NULL) {
        fprintf(stderr, "Failed to open file %s: %d\n", file_path, ret);
        return RET_FAIL;
    }

    if (fseek(fd, 0, SEEK_END)) {
        fprintf(stderr, "fseek() failed on file %s\n", file_path);
        ret = RET_FAIL;
        goto done;
    }

    *buf_size = ftell(fd);
    if (*buf_size <= 0) {
        fprintf(stderr, "invalid file size: %s\n", file_path);
        ret = RET_FAIL;
        goto done;
    }

    *buf = (uint8_t*)malloc(*buf_size);
    if(*buf == NULL) {
        fprintf(stderr, "Out of memory!\n");
        ret = RET_FAIL;
        goto done;
    }

    /* Don't forget to go back to the beginning of the file. */
    fseek(fd, 0, SEEK_SET);
    if (fread(*buf, 1, *buf_size, fd) != *buf_size) {
        fprintf(stderr, "fread() failed on file %s\n", file_path);
        ret = RET_FAIL;
    }

done:
    fclose(fd);

    if (ret != RET_OK && buf != NULL) {
        free(*buf);
        *buf = NULL;
    }

    return ret;
}

static int32_t write_buf_to_file(void *mem_stream, const char *file_path)
{
    int32_t ret = RET_OK;

    /* either of these two methods should work fine */
#if 0
    const uint8_t *buf  = NULL;
    int32_t buf_size    = 0;

    /* Retrieve a const pointer to the memory buffer.
    There's no need to free at this point. */
    ret = mz_stream_mem_get_buffer(mem_stream, (const void**)&buf);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to get zip memory buffer: %d\n", ret);
    }

    if (buf == NULL) {
        fprintf(stderr, "Invalid zip memory buffer\n");
        return RET_FAIL;
    }
    else {
        mz_stream_mem_get_buffer_length(mem_stream, &buf_size);

        fprintf(stderr, "buf_size = %d\n",buf_size);
        FILE *fd = fopen(file_path, "w+");
        if (fd == NULL) {
            fprintf(stderr, "fopen() failed to open file %s\n", file_path);
            return RET_FAIL;
        }

        if (fwrite(buf, 1, buf_size, fd) != buf_size) {
            fprintf(stderr, "fwrite() failed to write to file %s\n", file_path);
            ret = RET_FAIL;
        }

        fclose(fd);
    }
#else
    unsigned char buf[UINT16_MAX] = { 0 };
    int read = 0, written = 0;

    FILE *fd = fopen(file_path, "wb+");
    if (fd == NULL) {
        fprintf(stderr, "fopen() failed to open file %s\n", file_path);
        return RET_FAIL;
    }

    mz_stream_mem_seek(mem_stream, 0, MZ_SEEK_SET);
    while ((read = mz_stream_mem_read(mem_stream, buf, sizeof(buf))) > 0) {
        written = fwrite(buf, 1, read, fd);
        if (written != read) {
            fprintf(stderr, "fwrite() failed to write to file %s\n", file_path);
            fclose(fd);
            return RET_FAIL;
       }
    }

    if (read != 0) {
        fprintf(stderr, "Failed to read inner memory stream\n");
        ret = RET_FAIL;
    }

    fclose(fd);
#endif /* #if 0 */

    return ret;
}

static int32_t normal_open(my_zip_archive *archive, int32_t open_mode)
{
    int32_t ret = RET_OK;
    archive->mz_stream = mz_stream_os_create();
    if (archive->mz_stream == NULL) {
        fprintf(stderr, "Failed to create stream\n");
        return RET_FAIL;
    }

    ret = mz_stream_os_open(archive->mz_stream, archive->file_path, open_mode);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open file %s: %d\n", archive->file_path, ret);
        return ret;
    }

    ret = mz_zip_open(archive->mz_handle, archive->mz_stream, open_mode);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open zip: %d\n", ret);
    }

    return ret;
}

static int32_t bufstream_open(my_zip_archive *archive, int32_t open_mode)
{
    int32_t ret = RET_OK;

    archive->buf_stream = mz_stream_buffered_create();
    if (archive->buf_stream == NULL) {
        fprintf(stderr, "Failed to create buffered stream\n");
        return RET_FAIL;
    }

    archive->mz_stream = mz_stream_os_create();
    if (archive->mz_stream == NULL) {
        fprintf(stderr, "Failed to create stream\n");
        return RET_FAIL;
    }

    mz_stream_set_base(archive->buf_stream, archive->mz_stream);

    /* this will also call mz_stream_os_open */
    ret = mz_stream_buffered_open(archive->buf_stream, archive->file_path, open_mode);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open file %s: %d\n", archive->file_path, ret);
        return ret;
    }

    ret = mz_zip_open(archive->mz_handle, archive->buf_stream, open_mode);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open zip: %d\n", ret);
    }

    return ret;
}

static int32_t memory_open(my_zip_archive *archive, int32_t open_mode)
{
    int32_t ret = RET_OK;

    archive->mz_stream = mz_stream_mem_create();
    if (archive->mz_stream == NULL) {
        fprintf(stderr, "Failed to create memory stream\n");
        return RET_FAIL;
    }

    if (!(open_mode & MZ_OPEN_MODE_CREATE)) {
        /* This wouldn't make much sense in a real use-case.
           It's for demonstration purposes only. */
        ret = read_file_into_buf(archive->file_path, &archive->buf, &archive->buf_size);
        if (ret != RET_OK) {
            return ret;
        }

        /* Even if minizip now has OUR buffer, it's still our
        responsibility to free it, so we must keep the pointer. */
        mz_stream_mem_set_buffer(archive->mz_stream, archive->buf, archive->buf_size);
    }
    else {
        /* file doesn't exist yet, nothing to read from.
        mz_stream_mem_set_grow_size(archive->mz_stream, (128 * 1024)); */
    }

    ret = mz_stream_open(archive->mz_stream, NULL, open_mode);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open file %s: %d\n", archive->file_path, ret);
        return ret;
    }

    ret = mz_zip_open(archive->mz_handle, archive->mz_stream, open_mode);
    if (ret != MZ_OK) {
        fprintf(stderr, "Failed to open zip: %d\n", ret);
    }

    return ret;
}

int32_t open_archive(my_zip_archive *archive, const int8_t archive_mode, int32_t open_mode)
{
    int32_t ret = RET_OK;

    archive->mode = archive_mode;

    archive->mz_handle = mz_zip_create();
    if (archive->mz_handle == NULL) {
        fprintf(stderr, "Failed to create handle\n");
        return RET_FAIL;
    }

    switch (archive->mode) {
        case MODE_NORMAL:
            ret = normal_open(archive, open_mode);
            break;

        case MODE_BUFSTREAM:
            ret = bufstream_open(archive, open_mode);
            break;

        case MODE_MEMORY:
            ret = memory_open(archive, open_mode);
            break;
    }

    return ret;
}

int32_t close_archive(my_zip_archive *archive)
{
    int32_t ret = RET_OK;

    if (archive->mz_handle != NULL) {
        mz_zip_close(archive->mz_handle);
        mz_zip_delete(&archive->mz_handle);
        archive->mz_handle = NULL;
    }

    switch (archive->mode) {
        case MODE_NORMAL:
            if (archive->mz_stream != NULL) {
                ret = mz_stream_os_close(archive->mz_stream);
                if (ret != MZ_OK) {
                    fprintf(stderr, "Failed to close stream: %d\n", ret);
                }
                mz_stream_os_delete(&archive->mz_stream);
                archive->mz_stream = NULL;
            }
            break;

        case MODE_BUFSTREAM:
            if (archive->buf_stream != NULL) {
                ret = mz_stream_buffered_close(archive->buf_stream);
                if (ret != MZ_OK) {
                    fprintf(stderr, "Failed to close buffered stream: %d\n", ret);
                    ret = RET_FAIL;
                }

                /* We need to free mz_stream ourselves.
                mz_stream_buffered_delete() doesn't free it, even though it's set as base stream.
                by calling mz_stream_set_base(). This could be a bug. */
                if (archive->mz_stream != NULL) {
                    mz_stream_os_delete(&archive->mz_stream);
                }

                mz_stream_buffered_delete(&archive->buf_stream);
                archive->buf_stream = NULL;
            }
            break;

        case MODE_MEMORY:
            if (archive->buf != NULL) {
                free(archive->buf);
                archive->buf = NULL;
            }
            else {
                /* This is the memory stream for the target file */

                /* zip_handle MUST be closed before we can do this. Otherwise,
                the resulting zip archive will be incomplete, since the
                central directory data is written upon closure. */
                ret = write_buf_to_file(archive->mz_stream, archive->file_path);
            }

            if (archive->mz_stream != NULL) {
                mz_stream_mem_delete(&archive->mz_stream);
            }
            break;
    }

    return ret;
}
