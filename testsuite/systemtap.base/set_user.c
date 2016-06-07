#include <unistd.h>
#include <stdlib.h>
#include <string.h>
int main()
{
  char buf[1024];
  memset(buf, 'a', sizeof(buf));
  write(1, buf, 1024);
  exit(0);
}
