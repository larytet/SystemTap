// Must compile with -g -O

// Note that there is no 100% guarantee that GCC will inline these. But giving
// really simple functions should help it make its decision. But note that they
// can't be too simple either, otherwise we won't even have an inline instance.

__attribute__((always_inline))
inline int baz(int a, int b) {
   return a + b;
}

__attribute__((always_inline))
inline int foo(int a, int b) {
   int c = a*2 + b;
   c = baz(b, c);
   return c;
}

__attribute__((always_inline))
inline int bar(int a, int b) {
   int c = a + b;
   c = foo(c, a*b);
   return c;
}

int main(int argc, char** argv) {
   int a = argc;
   a = foo(a, a);
   a = bar(a, a);
   a = foo(a, a);
   return ((a > 100) ? 0 : 1);
}

