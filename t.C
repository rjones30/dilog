#include <dilog.h>

int main() {
   for (int i=0; i < 10; ++i) {
      dilog::get("mytrun").printf("iteration %d\n",i);
   }
   return 0;
}
