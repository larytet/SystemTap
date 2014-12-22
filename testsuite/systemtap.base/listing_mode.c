#include "sys/sdt.h"
#include "stdio.h"

#ifdef LISTING_MODE_MAIN

int globalvar = 1;

extern libfoo(int lf);

__attribute__((always_inline))
inline int inln(int i) {

   return i % 5;
}

int bar(int b) {

   int i = inln(b);
   printf("inln returned %d\n", i);
   return i * 3;
}

int foo(int f) {

   int b = bar(f);
   return b * 2;
}

int main(void) {

   globalvar = foo(globalvar);
   globalvar = libfoo(globalvar);
main_label:
   STAP_PROBE1(main, mark, globalvar);
   while (1) {sleep(5000);}
   return 0;
}

#elif LISTING_MODE_LIB

__attribute__((always_inline))
inline int libinln(int i) {

   return i % 5;
}

int libbar(int lb) {

   int i = libinln(lb);
   printf("inln returned %d\n", i);
   return i * 3;
}

int libfoo(int lf) {

   int lb = libbar(lf);
lib_label:
   STAP_PROBE1(lib, mark, lb);
   return lb * 2;
}

#else
#error must define LISTING_MODE_MAIN or LISTING_MODE_LIB
#endif
