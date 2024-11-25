#include "types.h"
#include "user.h"

int snoozetime = 5;
void dummy() {
  printf(1,"This function is only here so snooze doesn't end up at address 0!\n");
}

void snooze(int signum) {
  // this function deliberately overwrites a register
  // value. The main function keeps important data in 
  // %rbx, and in correctly implemented signal handling,
  // that data should not be overwritten by this bit of asm
  asm volatile("xor %%ebx,%%ebx":::);

  printf(1,"Yawn... another %d seconds?\n",snoozetime--);
  if(snoozetime==0) 
    signal(14,0);
  alarm(1);
}

int main(int argc, char** argv) {
  signal(14,snooze);
  alarm(3);
  
  // "register" forces i and j to reside in registers
  // rather than on the stack. See snooze(). 
  register int i=0,j=0;
  
  while(1) {
    printf(1,"Still looping %d, %d...\n",
      i++,j++);
    sleep(100);
  }
}

