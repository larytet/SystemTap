#include <iostream>
#include <cstdio>

extern "C" {
#include <unistd.h>
}

using namespace std;

void echo(const char *foo) {
  printf("%s\n", foo);
}

int main() {
  cout << "my pid is " << getpid() << endl;

  string line;
  while (getline(cin,line)) echo(line.c_str());
}
