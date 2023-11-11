#include <dilog.h>
#include <iostream>

int main() {
   for (int i=0; i < 10; ++i) {
      dilog::block myloop("mytrun", "myloop");
      dilog::get("mytrun").printf("iteration %d\n",i);
   }
   std::cout << "test successful!" << std::endl;
   return 0;
}
