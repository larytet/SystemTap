#define _GNU_SOURCE             /* For pthread_getattr_np */
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

static pthread_once_t printed_p = PTHREAD_ONCE_INIT;
void print_it () 
{
  int rc;
  pthread_attr_t foo;
  size_t size;
  rc = pthread_getattr_np (pthread_self(), & foo);
  assert (rc == 0);
  rc = pthread_attr_getstacksize(&foo, &size);
  assert (rc == 0);
  printf ("stacksize=%u\n", size);
  rc = pthread_attr_destroy (&foo);
  assert (rc == 0);
}

void *tfunc(void *arg)
{
  /* Choose some random thread to print stack size */
  (void) pthread_once(&printed_p, &print_it);
  return NULL;
}

 
#define MAXTHREADS 4096

int
main(int argc, char **argv)
{
    pthread_t thr[MAXTHREADS];
    pthread_attr_t attr;
    int numthreads;
    int stacksize;
    int rc;
    int threads_created = 0;
    size_t i;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s numthreads stacksize|0\n", argv[0]);
	return -1;
    }

    numthreads = atoi(argv[1]);
    if (numthreads > MAXTHREADS) {
	numthreads = MAXTHREADS;
    }
    stacksize = atoi(argv[2]);

    rc = pthread_attr_init(&attr);
    assert (rc == 0);

    if (stacksize > 0) {
      rc = pthread_attr_setstacksize(&attr, (size_t) stacksize);
      assert (rc == 0);
    }

    for (i = 0; i < numthreads; i++) {
      rc = pthread_create(&thr[i], (stacksize == 0 ? NULL : &attr), tfunc, NULL);

      /* On systems with not enough memory, pthread_create() can fail
       * after creating lots of threads. Just ignore this error (if
       * we've already created at least 100 threads). */
      if (rc == EAGAIN && threads_created > 100)
	  break;
      assert (rc == 0);
      threads_created++;
    }

    /* Wait for all the threads to finish (otherwise we can exit
     * before one of our threads has the chance to print the stack
     * size). */
    for (i = 0; i < threads_created; i++) {
	pthread_join(thr[i], NULL);
    }

    rc = pthread_attr_destroy(&attr);
    assert (rc == 0);

    return 0;
}

