// user/kpanic.c
// Chương trình user gọi syscall kpanic để test panic visualizer.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  printf("user: Calling kpanic() syscall...\n");
  
  // Gọi vào kernel. Kernel sẽ không bao giờ quay trở lại.
  kpanic();

  // Dòng này không bao giờ nên được in ra
  printf("user: kpanic() returned (This should not happen!)\n");
  
  exit(0);
}