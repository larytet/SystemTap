#include <pthread.h>
#include <unistd.h>
#include "sys/sdt.h"

extern int libloopfunc (void);

/* Thread entry point */
void *bar (void *b) {
  int i;
  int *j = (int *)b;
  for (i = 0; i < 10; ++i)
    *j += i;
 a:
  return b;
}

/* We need an inline function. */
inline int ibar (void) {
  return libloopfunc ();
}

/* We need a threaded app.  It should not be inline,
   so a .callee("tbar") test will require the GNU_call_site* DWARF goo
   to match, not just the presence of a mere DW_AT_inline=3 case. */
int tbar (void) {
  void *x;
  int j = 0;
  STAP_PROBE(_test_, main_enter);
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_create (& thread, & attr, bar, (void*)& j);
  pthread_join (thread, & x);
  return j;
}

int main (int argc, char *argv[]) {
  int j = 0;
  /* Don't loop if an argument was passed */
  /* We split into two cases rather than check in a single loop because
   * GCC can output screwy debuginfo otherwise (RHBZ1092144) */
  if (argc > 1) {
    j += ibar ();
    j += tbar ();
    return 0;
  }
  for (;;) {
    j += ibar ();
    j += tbar ();
#if !defined NOSLEEP
    usleep (250000); /* 1/4 second pause.  */
#endif
  }
  return j;
}
