#include <unistd.h>

int main()
{
   char buffer[7] = "foobar\n";
   write(1, buffer, 7);
    
   char buffer2[2] = {'\0','\n'};
   write(1, buffer2, 2);

   return 0;
}
