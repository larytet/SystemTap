#include <stdio.h>

__attribute__((always_inline))
inline static void foo(int i)
{
   printf("printf %d\n", i);
}

int main(int argc, char** argv)
{
   foo(argc);
   foo(argc*2);
   return 0;
}
