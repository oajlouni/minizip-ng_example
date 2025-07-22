# minizip-ng_example
[Minizip-ng](https://github.com/zlib-ng/minizip-ng) is a fork of [Minizip](https://github.com/madler/zlib/tree/master/contrib/minizip); the zip manipulation library found in the zlib distribution.

I created this repository as a basic example that showcases some of minizip-ng's capabilities and how to use them.

##Motivation
While the repository is actively maintained and has decent [documentation](https://raw.githubusercontent.com/zlib-ng/minizip-ng/636cba864390d51671779fd32a3fc9a0ef5c7735/doc/README.md), getting up and running wasn't exactly straightforward, due to the lack of basic examples.

I looked at [one](https://gist.githubusercontent.com/chenxiaolong/dbab3fbef51b9d0fa969e220dbb85967/raw/23c9c202431f86268e124d65f6a54256e0ea34d6/minizip_buf_hang_test.c) of the examples included in the docs and it served as a good starting point, even though it's outdated and doesn't compile with the latest version of minizip-ng.

I reworked the example and include additional functionality, which hopefully is easy to follow and understand.

The code covers the use of [streams](https://github.com/zlib-ng/minizip-ng/blob/636cba864390d51671779fd32a3fc9a0ef5c7735/doc/README.md#using-streams), including buffered and memory streams. I also provided some comments in the code for things to consider while using the library.

##Build
To compile and run the code, you need to also have minizip-ng. You could build it as a static library to keep the process simple. On Linux with gcc, I use something like:

```
gcc  -I/path/to/minizip-ng mzip_example.c my_zip_archive.c /path/to/libminizip-ng.a -o minizip-ng_example
```
