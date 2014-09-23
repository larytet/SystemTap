#include "sys/sdt.h"

struct foo {
   int bar;
   int zoo;
   int poo;
};

int globalvar = 1;

#ifdef SDT_VARNAME_MAIN

int main_globalvar = 2;
static int localvar = 3;
static struct foo localstruct = { 4, 5, 6 };

extern int foo();
int main() {
   STAP_PROBE6(main, mark, globalvar,
                           main_globalvar,
                           localvar,
                           localstruct.bar,
                           localstruct.zoo,
                           localstruct.poo);
   foo();
   return 0;
}

#elif SDT_VARNAME_LIB

int lib_globalvar = 7;
static int localvar = 8;
static struct foo localstruct = { 9, 10, 11 };

int foo()
{
   STAP_PROBE6(lib, mark, globalvar,
                          lib_globalvar,
                          localvar,
                          localstruct.bar,
                          localstruct.zoo,
                          localstruct.poo);
   return localvar;
}

#else
#error must define SDT_VARNAME_MAIN or SDT_VARNAME_LIB
#endif

