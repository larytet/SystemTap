// Must compile with -g and -O

int foo (int, int);

int bar (int x, int y) {
   int z = x % y;
   return z;
}

int main(int argc, char** argv) {
   int a = argc;
   a = foo(a, a);
   return a;
}

