//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicking = 0; // printing a panic message
volatile int panicked = 0; // spinning forever at end of a panic

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(long long xx, int base, int sign)
{
  char buf[20];
  int i;
  unsigned long long x;

  if(sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console.
int
printf(char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  if(panicking == 0)
    acquire(&pr.lock);

  va_start(ap, fmt);
  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      consputc(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0) c1 = fmt[i+1] & 0xff;
    if(c1) c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      printptr(va_arg(ap, uint64));
    } else if(c0 == 'c'){
      consputc(va_arg(ap, uint));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
    } else if(c0 == '%'){
      consputc('%');
    } else if(c0 == 0){
      break;
    } else {
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c0);
    }

  }
  va_end(ap);

  if(panicking == 0)
    release(&pr.lock);

  return 0;
}

//-----Code moi-------------------------

// Mã màu ANSI
#define RED     "\x1b[31m"
#define YELLOW  "\x1b[33m"
#define CYAN    "\x1b[36m"
#define MAGENTA "\x1b[35m"
#define RESET   "\x1b[0m"

// Địa chỉ thiết bị test của QEMU
#define QEMU_TEST_CTRL (volatile uint32*)(0x100000)
#define QEMU_TEST_FAIL 0x3333 

// Hàm tạo độ trễ giả (Busy wait)
void
panic_spin_delay(uint64 count)
{
  volatile uint64 i;
  for(i = 0; i < count; i++)
    ;
}

void
panic(char *s)
{
  uint64 sepc = r_sepc();
  uint64 scause = r_scause();
  struct proc *p = myproc();

  // 1. Hiển thị Log
  printf(RED "\n\n--- KERNEL PANIC ---\n" RESET);
  printf(RED "REASON: %s\n\n" RESET, s);
  
  printf(YELLOW "--- CPU STATE ---\n" RESET);
  printf(YELLOW "sepc:   0x%p\n" RESET, (void*)sepc);
  printf(YELLOW "scause: 0x%p" RESET, (void*)scause);

  if(scause == 12) printf(" (Instruction Page Fault)\n");
  else if(scause == 13) printf(" (Load Page Fault)\n");
  else if(scause == 15) printf(" (Store/AMO Page Fault)\n");
  else printf(" (Unknown Cause)\n");

  if(p) {
    printf(CYAN "\n--- PROCESS STATE ---\n" RESET);
    printf(CYAN "PID:    %d\n" RESET, p->pid);
    printf(CYAN "Name:   %s\n" RESET, p->name);
  } else {
    printf(CYAN "\n--- PROCESS STATE ---\n" RESET);
    printf(CYAN "No process context\n" RESET);
  }
  
  printf(RED "\n--- SYSTEM HALTED ---\n" RESET);
  
  panicked = 1; 

  // 2. Auto-Reboot Sequence (Đếm ngược)
  printf(MAGENTA "\nAuto-rebooting system in 3 seconds...\n" RESET);
  
  // Đếm ngược 3..2..1
  for(int i = 3; i > 0; i--){
      printf(MAGENTA "%d...\n" RESET, i);
      // Delay khoảng 1 giây (số vòng lặp tùy thuộc tốc độ máy host)
      panic_spin_delay(200000000); 
  }

  printf(MAGENTA "Rebooting now!\n" RESET);

  // 3. Trigger QEMU Exit
  *QEMU_TEST_CTRL = QEMU_TEST_FAIL;

  for(;;)
    ;
}
 
//----------------Het code moi----------


void
printfinit(void)
{
  initlock(&pr.lock, "pr");
}
