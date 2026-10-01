#include"types.h"
#include"user.h"
int main(int argc, char** argv) {
  printf(1,"About to make first mmap. Next, you should see the first sentence from README\n");
  int fd = open("README",0);
  char* text = mmap(fd,0);
  if(text!=(void*)0x0000400000000000)
    printf(1,"Returned pointer is %p, should be 0x0000400000000000\n",text);
  text[85]=0;
  printf(1,"%s\n",text);

  printf(1,"\nSecond mmap coming\n");
  int fd2 = open("LARGE",0);
  char* text2 = mmap(fd2,0);

  if(text2!=(void*)0x0000400000001000)
    printf(1,"Returned pointer is %p, should be 0x0000400000001000\n",text2);
  text2[71680]=0;
  printf(1,"%s\n",text2+71661);

  printf(1,"Checking that first mmap is still ok, you should see the first sentence from README\n");
  if(text!=(void*)0x0000400000000000)
    printf(1,"Returned pointer is %p, should be 0x0000400000000000\n",text);
  text[85]=0;
  printf(1,"%s\n",text);

  exit();
}
