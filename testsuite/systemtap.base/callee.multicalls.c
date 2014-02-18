// Must compile with -g -O

__attribute__((noinline))
int baz(int a, int b) {
   return a-b + a*b;
}

__attribute__((noinline))
int foo(int a, int b) {
   return baz(a*2, b-a);
}

__attribute__((noinline))
int bar(int a, int b) {
   return foo(b*3, a);
}

int main(void) {
   int a = 1;
   a = foo(a, a);
   a = bar(a, a);
   a = foo(a, a);
   return a;
}

