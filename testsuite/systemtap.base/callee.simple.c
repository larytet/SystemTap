// Must compile with -g -O

__attribute__((noinline))
int level3(int a, int b) {
   return a + b;
}

__attribute__((noinline))
int level2(int a, int b) {
   return level3(a-b, a+b);
}

__attribute__((noinline))
int level1(int a, int b) {
   return level2(a/b, a%b);
}

int main(void) {
   int a = 1;
   a = level1(a, a);
   return a;
}

