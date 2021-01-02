#include <dilog.h>

int main() {
   for (int i=0; i < 10; ++i) {
      dilog::get("mytrun").myprintf("iteration %d\n",i);
   }
   return 0;
}
