#include <algorithm>
#include <string>

bool test_method() {
  std::string foo("foobar");
  std::string::iterator it = std::find(foo.begin(), foo.end(), 'b');
  return it == foo.begin() + 3;
}

int main(int argc, char **argv) {
  if (test_method()) {
    return 0;
  }
  return 1;
}
