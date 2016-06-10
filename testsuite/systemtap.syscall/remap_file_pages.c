/* COVERAGE: remap_file_pages */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
    int fd, pgsz;
    struct stat s;
    char *data;
    char f[] = "remap_file_pages_test_file";

    fd = open(f, O_RDWR|O_CREAT, 0777);
    write(fd, "blah", 4);
    fstat (fd, &s);
    data = mmap(0, s.st_size, PROT_READ, MAP_SHARED, fd, 0);
    pgsz = sysconf(_SC_PAGESIZE);

    remap_file_pages(data, pgsz, PROT_NONE, 0, MAP_SHARED);
    //staptest// remap_file_pages (XXXX, XXXX, PROT_NONE, XXXX, MAP_SHARED) = NNNN

    close(fd);
    unlink(f);

    // Limit testing

    remap_file_pages((void *)-1, pgsz, PROT_NONE, 0, MAP_SHARED);
#if __WORDSIZE == 64
    //staptest// remap_file_pages (0xffffffffffffffff, XXXX, PROT_NONE, XXXX, MAP_SHARED) = -NNNN
#else
    //staptest// remap_file_pages (0xffffffff, XXXX, PROT_NONE, XXXX, MAP_SHARED) = -NNNN
#endif

    remap_file_pages(data, (size_t)-1, PROT_NONE, 0, MAP_SHARED);
#if __WORDSIZE == 64
    //staptest// remap_file_pages (XXXX, 0xffffffffffffffff, PROT_NONE, XXXX, MAP_SHARED) = -NNNN
#else
    //staptest// remap_file_pages (XXXX, 0xffffffff, PROT_NONE, XXXX, MAP_SHARED) = -NNNN
#endif

    remap_file_pages(data, pgsz, -1, 0, MAP_SHARED);
    //staptest// remap_file_pages (XXXX, XXXX, PROT_[^ ]+|XXXX, XXXX, MAP_SHARED) = -NNNN

    remap_file_pages(data, pgsz, PROT_NONE, (ssize_t)-1, MAP_SHARED);
#if __WORDSIZE == 64
    //staptest// remap_file_pages (XXXX, XXXX, PROT_NONE, 0xffffffffffffffff, MAP_SHARED) = NNNN
#else
    //staptest// remap_file_pages (XXXX, XXXX, PROT_NONE, 0xffffffff, MAP_SHARED) = NNNN
#endif

    remap_file_pages(data, pgsz, PROT_NONE, 0, -1);
    //staptest// remap_file_pages (XXXX, XXXX, PROT_NONE, XXXX, MAP_[^ ]+|XXXX) = NNNN


    return 0;
}

