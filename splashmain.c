#include"types.h"
#include"x86.h"

void
waitdisk(void)
{
  while((inb(0x1F7) & 0xC0) != 0x40);
}

void
readsect(char* out, int sector)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1); 
  outb(0x1F3, sector);
  outb(0x1F4, sector>>8);
  outb(0x1F5, sector>>16);
  outb(0x1F6, 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  insl(0x1F0, (void*)out, 512/4);
}

void splashmain() {
    for(int i=0;i<64000;i+=512) {
        readsect((void*)(0xa0000+i),i/512+1);
    }
}
