#include <dilog.h>

int main() {
   for (int i=0; i < 10; ++i) {
      dilog::block myloop("mytrun", "myloop");
      dilog::get("mytrun").printf("iteration %d\n",i);
   }
   return 0;
}
