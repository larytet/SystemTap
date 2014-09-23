#include "stdio.h"
#include "sys/sdt.h"

void third(){}
void second(){third();}
void first(){second();}

int main()
{
  int rc = 0;
  l1:
    STAP_PROBE(process_by_cmd, main_start);
  first();
  STAP_PROBE(process_by_cmd, main_end);
  return rc;
}
