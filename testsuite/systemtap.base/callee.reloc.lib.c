// Must compile with -g -O

__attribute__((noinline))
int bar(int a, int b) {
   int c;
   c = a % b;
   return c;
}

int foo(int a, int b) {
   int c;
   c = bar(b-a, b*a);
   return c;
}
