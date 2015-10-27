/* COVERAGE: acct */
#include <unistd.h>

int main()
{
  acct("foobar");
  //staptest// acct ("foobar") = -NNNN

  acct((char *)-1);
#ifdef __s390__
  //staptest// acct (0x[7]?[f]+) = -NNNN
#else
  //staptest// acct (0x[f]+) = -NNNN
#endif

  return 0;
}
