#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
int main()
{
  char cbuf[64];
  
  mlockall(MCL_CURRENT);
  memset(cbuf,'a',sizeof(cbuf));
  exit(0);
}
