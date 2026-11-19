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
#include "stat.h"
#include "buf.h"
#include "memlayout.h"
#include "iso9660.h"

  static void 
read_block_range(char* dst, uint start_block, uint count) {
  for (int block=start_block;block<start_block+count;block++) {
      struct buf *bp = bread(2, block);
      memmove(dst,bp->data,BSIZE);
      brelse(bp);
      dst+=BSIZE;
  }
}

  void
iso9660fs_ipopulate(struct inode* ip)
{
    ip->type = T_FILE;
    ip->size = 100;
    ip->flags |= I_VALID;
}

  void
iso9660fs_iupdate(struct inode* ip)
{
}

  static int
iso9660fs_writei(struct inode* ip, char* buf, uint offset, uint count)
{
  return -1;
}

  int
iso9660fs_readi(struct inode* ip, char* dst, uint offset, uint size)
{
  if ( ip->type == T_DIR ) {
    if(offset==0) {
      struct dirent *de = (struct dirent*)dst;
      memmove(de->name,"FAKE.txt",9);
      de->inum = 1;
      return sizeof(struct dirent);
    }
    else {
      return 0;
    }
  }
  else if ( ip->type == T_FILE) {
    int len = size<16?size:16;
    if (offset < ip->size) {
      memmove(dst,"Very Fake Text.",len);
      return len;
    }
    else {
      return 0;
    }
  }
  return -1; // return -1 on error
}

struct inode_functions iso9660fs_functions = {
  iso9660fs_ipopulate,
  iso9660fs_iupdate,
  iso9660fs_readi,
  iso9660fs_writei,
};

  void
iso9660fsinit(char * const path, char * device)
{
  /*

      uint dev = ip->dev;
    uint addr = ip->inum;  
    // because we're not sure about the alignment of the record, always read two blocks
    static char buf[1024];
    read_block_range(buf,addr/512,2);
    struct iso9660_dir_s *entry = buf+(addr & 0x1ff); // offset into the block

    ip->type = ((entry->file_flags == 0x2)?T_DIR:T_FILE);
    ip->nlink = 1;
    ip->size = entry->size;
    ip->addrs[0] = entry->extent*2048;

*/
  cprintf("Mounting cdrom\n");
  begin_op();
  struct inode* mount_point = namei(path);
  if (mount_point) {

    static struct iso9660_pvd_s pvd;
    read_block_range((char*)&pvd,64,4);
    if(memcmp(pvd.id,"CD001",5)==0) {
      cprintf("ID matched ISO9660 file system\n");
    }
    else {
      cprintf("ID mismatch\n");
      return;
    }
  
  
    cprintf("Root directory record version %d len %d size %d at %d, volume size %d block size %d set size %d.\n",pvd.version,
    pvd.root_directory_record.length,
    pvd.root_directory_record.size,
    pvd.root_directory_record.extent,pvd.volume_space_size*2048,pvd.logical_block_size,pvd.volume_set_size);
    
    uint addr = pvd.root_directory_record.extent*2048; // location of directory file in bytes      
    static char buf[2048];
    read_block_range(buf,addr/512,4);
    struct iso9660_dir_s *entry = (struct iso9660_dir_s*)buf;

    while (entry->length) {
      char namebuf[100]={0};
      memmove(namebuf,&entry->filename.str[1],entry->filename.len);
      cprintf("Entry: %s\n",namebuf);
      entry = (struct iso9660_dir_s*) ((char*)entry+entry->length);
    }


    ilock(mount_point);
    mount_point->i_func = &iso9660fs_functions;
    mount_point->mounted_dev = 2;
    mount_point->addrs[0]=pvd.root_directory_record.extent*2048;    
    iunlock(mount_point);
  }
  end_op();
}
