#ifndef MY_ZIP_ARCHIVE_H
#define MY_ZIP_ARCHIVE_H

#define RET_OK          0
#define RET_FAIL        -1

#define MODE_NORMAL     1
#define MODE_BUFSTREAM  2
#define MODE_MEMORY     4

typedef struct _my_zip_archive
{
    const char *file_path;

    void    *mz_handle;
    void    *mz_stream;
    void    *buf_stream;

    int8_t   mode;

    /* Used in memory mode. The buffer is
    our responsibility, since we're creating it
    and not minizip. */
    uint8_t *buf;
    int32_t  buf_size;
} my_zip_archive;

int32_t open_archive(my_zip_archive *archive, const int8_t archive_mode, int32_t open_mode);
int32_t close_archive(my_zip_archive *archive);

#endif // #ifndef MY_ZIP_ARCHIVE_H
