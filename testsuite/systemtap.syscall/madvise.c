/* COVERAGE: madvise */

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char *str_for_file = "abcdefghijklmnopqrstuvwxyz12345\n";
    char filename[PATH_MAX];
    int i, fd;
    void *file;
    size_t size = 40960;

    /* Create a temporary file. */
    sprintf(filename, "%s-out.%d", *argv, getpid());
    fd = open(filename, O_RDWR | O_CREAT, 0664);

    /* Writing 40 KB of data into this file [32 * 1280 = 40960] */
    for (i = 0; i < 1280; i++)
	write(fd, str_for_file, strlen(str_for_file));

    /* Map the input file into memory */
    file = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    madvise((void *)-1, size, MADV_NORMAL);
    //staptest// madvise (0x[f]+, 40960, MADV_NORMAL) = -NNNN (EINVAL)

    /* Ignore return code */
    madvise(file, (size_t)-1, MADV_NORMAL);
#if __WORDSIZE == 64
    //staptest// madvise (XXXX, 18446744073709551615, MADV_NORMAL) = -NNNN
#else
    //staptest// madvise (XXXX, 4294967295, MADV_NORMAL) = -NNNN
#endif

    madvise(file, size, -1);
    //staptest// madvise (XXXX, 40960, 0xffffffff) = -NNNN (EINVAL)

    madvise(file, size, MADV_NORMAL);
    //staptest// madvise (XXXX, 40960, MADV_NORMAL) = 0

    madvise(file, size, MADV_RANDOM);
    //staptest// madvise (XXXX, 40960, MADV_RANDOM) = 0

    madvise(file, size, MADV_SEQUENTIAL);
    //staptest// madvise (XXXX, 40960, MADV_SEQUENTIAL) = 0

    madvise(file, size, MADV_WILLNEED);
    //staptest// madvise (XXXX, 40960, MADV_WILLNEED) = 0

    madvise(file, size, MADV_DONTNEED);
    //staptest// madvise (XXXX, 40960, MADV_DONTNEED) = 0

#ifdef MADV_FREE
    /* Ignore return value */
    madvise(file, size, MADV_FREE);
    //staptest// madvise (XXXX, 40960, MADV_FREE)
#endif

    /* Ignore return value */
    madvise(file, size, MADV_REMOVE);
    //staptest// madvise (XXXX, 40960, MADV_REMOVE)

    madvise(file, size, MADV_DONTFORK);
    //staptest// madvise (XXXX, 40960, MADV_DONTFORK) = 0

    madvise(file, size, MADV_DOFORK);
    //staptest// madvise (XXXX, 40960, MADV_DOFORK) = 0

    /* NB: the following two can cause alarming kernel messages to appear
       in the logs.  Ignore these:
       [107552.743276] Injecting memory failure for page 7d42f at 55578000
       [107552.745627] MCE 0x7d42f: dirty LRU page recovery: Recovered
    */

#ifdef MADV_HWPOISON
    /* Ignore return value */
    madvise(file, size, MADV_HWPOISON);
    //staptest// madvise (XXXX, 40960, MADV_HWPOISON)
#endif

#ifdef MADV_SOFT_OFFLINE
    madvise(file, size, MADV_SOFT_OFFLINE);
    //staptest// madvise (XXXX, 40960, MADV_SOFT_OFFLINE) = 0
#endif

#ifdef MADV_MERGEABLE
    madvise(file, size, MADV_MERGEABLE);
    //staptest// madvise (XXXX, 40960, MADV_MERGEABLE) = 0
#endif

#ifdef MADV_UNMERGEABLE
    madvise(file, size, MADV_UNMERGEABLE);
    //staptest// madvise (XXXX, 40960, MADV_UNMERGEABLE) = 0
#endif

#ifdef MADV_HUGEPAGE
    /* Ignore return value */
    madvise(file, size, MADV_HUGEPAGE);
    //staptest// madvise (XXXX, 40960, MADV_HUGEPAGE)
#endif

#ifdef MADV_NOHUGEPAGE
    /* Ignore return value */
    madvise(file, size, MADV_NOHUGEPAGE);
    //staptest// madvise (XXXX, 40960, MADV_NOHUGEPAGE)
#endif

#ifdef MADV_DONTDUMP
    madvise(file, size, MADV_DONTDUMP);
    //staptest// madvise (XXXX, 40960, MADV_DONTDUMP) = 0
#endif

#ifdef MADV_DODUMP
    madvise(file, size, MADV_DODUMP);
    //staptest// madvise (XXXX, 40960, MADV_DODUMP) = 0
#endif

    /* Cleanup. */
    munmap(file, size);

    close(fd);
    unlink(filename);

    return 0;
}
