#include "types.h"
#include "param.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fs.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "memlayout.h"

#define T_DIR 1
#define T_FILE 2



  void
procfs_ipopulate(struct inode* ip)
{
  ip->size = 0;
  ip->flags |= I_VALID;
  ip->dev = 2;

  // inum < 10000 are reserved for directories
  // use inum > 10000 for files in procfs
  ip->type = ip->inum < 10002 ? T_DIR : T_FILE;  
  ip->mount_parent = 0;
}

  void
procfs_iupdate(struct inode* ip)
{
}

  static int
procfs_writei(struct inode* ip, char* buf, uint offset, uint count)
{
  return -1;
}

  static int
sprintuint(char* buf, uint x)
{
  uint stack[10];
  uint stack_size = 0;
  if (x == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return 0;
  }
  while (x) {
    stack[stack_size++] = x % 10u;
    x /= 10u;
  }
  uint buf_size = 0;
  while (stack_size) {
    buf[buf_size++] = '0' + stack[stack_size - 1];
    stack_size--;
  }
  buf[buf_size] = 0;
  return buf_size;
}

  static void
sprintx32(char * buf, uint x)
{
  buf[0] = x >> 28;
  for (int i = 0; i < 8; i++) {
    uint y = 0xf & (x >> (28 - (i * 4)));
    buf[i] = (y < 10) ? (y + '0') : (y + 'a' - 10);
  }
}

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

#define PROCFILES ((4))
struct dirent procfiles[PROCFILES+NPROC+1] = {
  {10001,"."},
  {10001,".."}, // this is a special entry as this is a mounted folder - the parent inode number is pointless
  {10003,"meminfo"}, {10004,"cpuinfo"}};

// returns the number of active processes, and updates the procfiles table
  static uint
updateprocfiles()
{
  int num = 0, index = 0;
  acquire(&ptable.lock);
  while (index < NPROC) {
    if (ptable.proc[index].state != UNUSED && ptable.proc[index].state != ZOMBIE) {
      procfiles[PROCFILES+num].inum = index+1;
      sprintuint(procfiles[PROCFILES+num].name,ptable.proc[index].pid);
      num++;
      // also checks if the process at [index] is the current process
      // if yes, create a "self" directory
      // TODO: your code here


    }
    index++;
  }
  release(&ptable.lock);
  return PROCFILES + num;
}

  static int
readi_helper(char * buf, uint offset, uint maxsize, char * src, uint srcsize)
{
  if (offset > srcsize)
    return -1;
  uint end = offset + maxsize;
  if (end > srcsize)
    end = srcsize;
  memmove(buf, src+offset, end-offset);
  return end-offset;
}

  int
procfs_readi(struct inode* ip, char* buf, uint offset, uint size)
{
  const uint procsize = sizeof(struct dirent)*updateprocfiles();
  // the mount point. There are two inodes representing this folder:
  // the folder on the parent device, which has mounted_parent set, and 
  // the root folder on the local "device", the procfs, which uses inum 10001 for the root
  if (ip->mount_parent || ip->inum==10001) {
    return readi_helper(buf, offset, size, (char *)procfiles, procsize);
  }

  // directory - can only be one of the process directories: no other folders in proc
  if (ip->type == T_DIR) {
    // It contains "name", "pid", "ppid", and "mappings".
    struct dirent dentries[6] = {
      {ip->inum,"."},
      {10001,".."},
      {10000+ip->inum*10+2,"name"}, 
      {10000+ip->inum*10+3,"pid"},
      {10000+ip->inum*10+4,"ppid"},
      {10000+ip->inum*10+5,"mappings"}};
    return readi_helper(buf,offset,size,(char*)dentries,sizeof(dentries));
  }

  // files
  char buf1[32];
  switch (((int)ip->inum)) {
    case 10003: // meminfo: print the number of free pages
      sprintuint(buf1, kmemfreecount());
      return readi_helper(buf, offset, size, buf1, strlen(buf1));
    case 10004: // cpuinfo: print the total number of cpus. See the 'ncpu' global variable
      memmove(buf1,"CPUs: ",6);
      return 6+sprintuint(buf1+6, cpunum());
    default: break;
  }

  // filling the content for all other files, meaning the files inside proc folders
  int pid = (ip->inum-10000) / 10;
  int file = (ip->inum-10000) % 10;
  struct proc* p = proc_for_pid(pid);
  int numlen;
  switch(file) {
    case 2: 
        return readi_helper(buf,offset,size,p->name,strlen(p->name)); 
    case 3:
        numlen = sprintuint(buf1,p->pid);
        return readi_helper(buf,offset,size,buf1,numlen); 
    case 4:
        numlen = sprintuint(buf1,p->parent->pid);
        return readi_helper(buf,offset,size,buf1,numlen); 
    case 5: 
        panic("proc/mappings not implemented!");
  }


  return -1; // return -1 on error
}

struct inode_functions procfs_functions = {
  procfs_ipopulate,
  procfs_iupdate,
  procfs_readi,
  procfs_writei,
};

  void
procfsinit(char * const path)
{
  begin_op();
  char namebuf[100];

  struct inode* mount_point = namei(path);
  if (mount_point) {
    ilock(mount_point);
    mount_point->i_func = &procfs_functions;
    mount_point->mounted_dev = 2;
    mount_point->mount_parent = nameiparent(path,namebuf);
    iunlock(mount_point);
  }
  end_op();
}
