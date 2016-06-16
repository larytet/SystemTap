#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sdt.h>
int main()
{
  char cbuf[64];
  
  mlockall(MCL_CURRENT);
  memset(cbuf,'a',sizeof(cbuf));
  STAP_PROBE(set_user.exe,point);
  exit(0);
}
