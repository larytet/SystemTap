// Must compile with -g -O

extern int foo(int, int);

int main(int argc, char** argv) {
   int a = argc;
   a = foo(a, 5);
   return a;
}
