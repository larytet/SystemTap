#include <stdio.h>

int foo(int a)
{
   int b = 2;
   return b + a * 3;
}

void bar(int b) { b += 2; printf("single line! %d\n", b); }

int main(int argc, char** argv)
{
   if (argc != 1)
      return 42;

   foo(argv[0][0]); bar(argv[0][1]); return 0; }
