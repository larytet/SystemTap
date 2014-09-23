// Must compile with -g -O

int bar(int, int);

int foo(int a, int b) {
   int c = b-a;
   c = bar(c, b/a);
   return c;
}

