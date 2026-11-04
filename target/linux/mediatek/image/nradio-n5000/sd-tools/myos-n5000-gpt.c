/* Ultra SD GPT changes only. No firmware or environment payload writes. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <linux/loop.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#define SECTOR 512
#define TABLE_SIZE 16384
#define STOCK_NAME "myos-stock-env"
#define SD_DEVICE "/dev/mmcblk0"
#define NATIVE_ROOT "/dev/mmcblk0p4"
#define OVERLAY_ALIGN 65536ULL
struct native_root {
 uint64_t offset, data_bytes, fs_bytes;
 const char *fstype;
};
static const unsigned char ultra_guid[16] = {
 0x4f,0x57,0x52,0x54,0x11,0x22,0x33,0x44,
 0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0x00
};
struct gpt {
 unsigned char primary[SECTOR], backup[SECTOR], table[TABLE_SIZE];
 uint64_t sectors, backup_lba;
 int backup_ok, p7, native;
};
static uint32_t u32(const unsigned char *p) {
 return (uint32_t)p[0] | (uint32_t)p[1]<<8 |
        (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint64_t u64(const unsigned char *p) {
 return (uint64_t)u32(p) | (uint64_t)u32(p+4)<<32;
}
static void put32(unsigned char *p, uint32_t n) {
 for (int i=0;i<4;i++) p[i]=(unsigned char)(n>>(8*i));
}
static void put64(unsigned char *p, uint64_t n) {
 for (int i=0;i<8;i++) p[i]=(unsigned char)(n>>(8*i));
}
static uint32_t crc32(const unsigned char *p, size_t size) {
 uint32_t crc=~0U;
 while (size--) {
  crc ^= *p++;
  for (int i=0;i<8;i++) crc=(crc>>1)^((crc&1)?0xedb88320U:0);
 }
 return ~crc;
}
static int io_read(int fd, void *p, size_t n, uint64_t pos) {
 return pread(fd,p,n,(off_t)pos)==(ssize_t)n ? 0 : -1;
}
static int io_write(int fd, const void *p, size_t n, uint64_t pos) {
 return pwrite(fd,p,n,(off_t)pos)==(ssize_t)n ? 0 : -1;
}
static int header_ok(const unsigned char *h) {
 unsigned char copy[SECTOR];
 if (memcmp(h,"EFI PART",8) || u32(h+8)!=0x10000 ||
     u32(h+12)!=92 || u32(h+80)!=128 || u32(h+84)!=128) return 0;
 memcpy(copy,h,SECTOR); memset(copy+16,0,4);
 return crc32(copy,92)==u32(h+16);
}
static int empty(const unsigned char *p) {
 for(int i=0;i<16;i++) if(p[i]) return 0;
 return 1;
}
static int named(const unsigned char *entry,const char *name) {
 for(int i=0;i<36;i++) {
  unsigned char c=(i<(int)strlen(name))?(unsigned char)name[i]:0;
  if(entry[56+i*2]!=c || entry[57+i*2]) return 0;
 }
 return 1;
}
static void set_name(unsigned char *entry,const char *name) {
 memset(entry+56,0,72);
 for(size_t i=0;i<strlen(name);i++) entry[56+i*2]=(unsigned char)name[i];
}
static int native_root_info(struct native_root *r) {
 unsigned char sb[48], fs[512]; uint64_t size=0, used, blocks=0;
 unsigned int shift=0; int fd=open(NATIVE_ROOT,O_RDONLY|O_NOFOLLOW);
 memset(r,0,sizeof(*r)); r->fstype="unknown";
 if(fd<0) return -1;
 if(ioctl(fd,BLKGETSIZE64,&size) || io_read(fd,sb,sizeof(sb),0) ||
    memcmp(sb,"hsqs",4)) {close(fd);return -1;}
 used=u64(sb+40);
 if(used<sizeof(sb) || used>UINT64_MAX-(OVERLAY_ALIGN-1)) {close(fd);return -1;}
 r->offset=(used+OVERLAY_ALIGN-1)&~(OVERLAY_ALIGN-1);
 if(r->offset>=size || size-r->offset<4096) {close(fd);return -1;}
 r->data_bytes=size-r->offset;
 if(io_read(fd,fs,sizeof(fs),r->offset+1024)) {close(fd);return -1;}
 close(fd);
 if(u32(fs)==0xf2f52010U) {
  shift=u32(fs+16); blocks=u64(fs+36); r->fstype="f2fs";
  if(shift!=12) return -1;
 } else if((u32(fs+56)&0xffff)==0xef53) {
  if(u32(fs+24)>6) return -1;
  shift=10+u32(fs+24); blocks=u32(fs+4); r->fstype="ext4";
  if(u32(fs+96)&0x80) blocks|=(uint64_t)u32(fs+336)<<32;
 }
 if(blocks>UINT64_MAX>>shift) return -1;
 r->fs_bytes=blocks<<shift;
 return r->fs_bytes<=r->data_bytes?0:-1;
}
static int native_running_root(void) {
 struct stat root,part;
 return !stat("/",&root) && !stat(NATIVE_ROOT,&part) &&
        S_ISBLK(part.st_mode) && root.st_dev==part.st_rdev;
}
static int native_overlay_matches(const char *source) {
 struct native_root r; struct stat part; struct loop_info64 loop;
 if(native_root_info(&r) || stat(NATIVE_ROOT,&part)) return 0;
 int fd=open(source,O_RDONLY|O_NOFOLLOW);
 if(fd<0) return 0;
 int ok=!ioctl(fd,LOOP_GET_STATUS64,&loop) &&
        loop.lo_rdevice==(uint64_t)part.st_rdev && loop.lo_offset==r.offset;
 close(fd); return ok;
}
static int board_sd(void) {
#ifdef MYOS_GPT_TEST
 return 1;
#else
 char buf[4096]={0}; int fd=open("/proc/device-tree/compatible",O_RDONLY);
 if(fd<0) return 0;
 ssize_t n=read(fd,buf,sizeof(buf)-1); close(fd);
 int found=0;
 for(ssize_t i=0;i<n;i++) {
  if(!strcmp(buf+i,"nradio,n5000")) found=1;
  while(i<n && buf[i]) i++;
 }
 if(!found) return 0;
 fd=open("/sys/class/block/mmcblk0/device/type",O_RDONLY);
 if(fd<0) return 0;
 memset(buf,0,sizeof(buf)); n=read(fd,buf,16); close(fd);
 return n>=2 && !strncmp(buf,"SD",2);
#endif
}
static int mounted_overlay(int native) {
#ifdef MYOS_GPT_TEST
 (void)native;
 return 1;
#else
 FILE *f=fopen("/proc/mounts","r"); char line[4096];
 if(!f) return 0;
 int ok=0;
 while(fgets(line,sizeof(line),f)) {
  char source[128],target[128],type[32];
  if(sscanf(line,"%127s %127s %31s",source,target,type)!=3 || strcmp(target,"/overlay")) continue;
  if(native) {
   if((!strcmp(type,"f2fs") || !strcmp(type,"ext4")) && native_overlay_matches(source)) ok=1;
  } else if(!strcmp(source,"/dev/mmcblk0p7") && !strcmp(type,"ext4")) ok=1;
 }
 fclose(f); return ok;
#endif
}
static int native_boot_mode(void) {
#ifdef MYOS_GPT_TEST
 return 1;
#else
 const char *cmd;
 if(!access("/usr/libexec/myos-n5000-ubenv",X_OK))
  cmd="/usr/libexec/myos-n5000-ubenv -o 1 -s boot_from_sd";
 else if(!access("/usr/bin/ubenv",X_OK))
  cmd="/usr/bin/ubenv -o 1 -s boot_from_sd";
 else return -1;
 FILE *f=popen(cmd,"r"); char line[256]; int mode=-1;
 if(!f) return -1;
 while(fgets(line,sizeof(line),f)) {
  int value;
  if(sscanf(line,"boot_from_sd = %d",&value)==1 && (value==0 || value==1)) mode=value;
 }
 if(pclose(f)) return -1;
 return mode;
#endif
}
static int native_layout(const struct gpt *g) {
 const unsigned char *p1=g->table,*p2=p1+128,*p3=p2+128,*p4=p3+128;
 if(!named(p2,"u-boot-env") || !named(p3,"kernel") || !named(p4,"rootfs")) return 0;
 if(empty(p2) || empty(p3) || empty(p4)) return 0;
 uint64_t end=33;
 for(int i=0;i<4;i++) {
  const unsigned char *p=g->table+i*128;
  if(!i && empty(p)) continue;
  if(!i && !named(p,"nradio_extend_memory")) return 0;
  uint64_t start=u64(p+32),last=u64(p+40);
  if(start<=end || last<start || last>g->sectors-34 || last>u64(g->primary+48)) return 0;
  end=last;
 }
 if(u64(p2+40)-u64(p2+32)+1!=2048 ||
    u64(p3+40)-u64(p3+32)+1!=262144 ||
    u64(p4+40)-u64(p4+32)+1<65536) return 0;
 for(int i=4;i<128;i++) if(!empty(g->table+i*128)) return 0;
 return 1;
}
static int load_gpt(int fd,struct gpt *g) {
 struct stat st; uint64_t bytes=0;
 if(fstat(fd,&st)) return -1;
 if(S_ISBLK(st.st_mode)) {
  if(ioctl(fd,BLKGETSIZE64,&bytes)) return -1;
 } else if(S_ISREG(st.st_mode)) bytes=(uint64_t)st.st_size;
 else return -1;
 if(bytes%SECTOR || bytes/SECTOR<213154) return -1;
 g->sectors=bytes/SECTOR;
 if(io_read(fd,g->primary,SECTOR,SECTOR) || !header_ok(g->primary) ||
    u64(g->primary+24)!=1 || u64(g->primary+72)!=2 ||
    u64(g->primary+40)!=34 || u64(g->primary+48)>g->sectors-34) return -1;
 if(io_read(fd,g->table,TABLE_SIZE,2*SECTOR) ||
    crc32(g->table,TABLE_SIZE)!=u32(g->primary+88)) return -1;
 g->native=native_layout(g);
 g->p7=0;
 if(!g->native) {
 if(memcmp(g->primary+56,ultra_guid,16)) return -1;
 static const uint64_t starts[]={34,8192,9216,17408,21504,87040};
 static const uint64_t ends[]={8191,9215,17407,21503,87039,213119};
 static const char *names[]={"bl2",NULL,"factory","fip","kernel","rootfs"};
 for(int i=0;i<6;i++) {
  unsigned char *e=g->table+i*128;
  if(empty(e) || u64(e+32)!=starts[i] || u64(e+40)!=ends[i] ||
     (names[i] && !named(e,names[i]))) return -1;
 }
 if(!named(g->table+128,"u-boot-env") &&
    !named(g->table+128,STOCK_NAME)) return -1;
 g->p7=!empty(g->table+6*128);
 if(g->p7) {
  unsigned char *e=g->table+6*128;
  if(u64(e+32)<=213119 || u64(e+40)<u64(e+32) ||
     u64(e+40)>g->sectors-34) return -1;
 }
 for(int i=7;i<128;i++) if(!empty(g->table+i*128)) return -1;
 }
 g->backup_lba=u64(g->primary+32);
 g->backup_ok=0;
 if(g->backup_lba>33 && g->backup_lba<g->sectors) {
  unsigned char table[TABLE_SIZE];
  if(!io_read(fd,g->backup,SECTOR,g->backup_lba*SECTOR) &&
     header_ok(g->backup) && u64(g->backup+24)==g->backup_lba &&
     u64(g->backup+32)==1 && u64(g->backup+72)==g->backup_lba-32 &&
     !memcmp(g->backup+56,g->primary+56,16) &&
     u64(g->backup+40)==u64(g->primary+40) &&
     u64(g->backup+48)==u64(g->primary+48) &&
     !io_read(fd,table,TABLE_SIZE,(g->backup_lba-32)*SECTOR) &&
     crc32(table,TABLE_SIZE)==u32(g->backup+88) &&
     !memcmp(table,g->table,TABLE_SIZE)) g->backup_ok=1;
 }
 return 0;
}
static void fix_header(unsigned char *h,const unsigned char *table) {
 put32(h+88,crc32(table,TABLE_SIZE));
 memset(h+16,0,4); put32(h+16,crc32(h,92));
}
static int commit(int fd,const struct gpt *old,struct gpt *next) {
 /* Backup first; the old primary remains usable until the final commit. */
 uint64_t end=next->sectors-1;
 unsigned char old_mbr[SECTOR],new_mbr[SECTOR]; int mbr_change=0;
 if(next->native) {
  if(io_read(fd,old_mbr,SECTOR,0) || old_mbr[510]!=0x55 || old_mbr[511]!=0xaa) return -1;
  memcpy(new_mbr,old_mbr,SECTOR);
  for(int i=0;i<4;i++) {
   unsigned char *p=new_mbr+446+i*16;
   if(p[4]==0xee && u32(p+8)==1 && !mbr_change) {
    put32(p+12,end>UINT32_MAX?UINT32_MAX:(uint32_t)end); mbr_change=1;
   } else if(p[4]) return -1;
  }
  if(!mbr_change) return -1;
 }
 next->backup_lba=end;
 put64(next->primary+32,end); put64(next->primary+48,end-33);
 memcpy(next->backup,next->primary,SECTOR);
 put64(next->backup+24,end); put64(next->backup+32,1);
 put64(next->backup+72,end-32);
 fix_header(next->primary,next->table); fix_header(next->backup,next->table);
 if(io_write(fd,next->table,TABLE_SIZE,(end-32)*SECTOR) ||
    io_write(fd,next->backup,SECTOR,end*SECTOR) || fsync(fd) ||
    io_write(fd,next->table,TABLE_SIZE,2*SECTOR) ||
    io_write(fd,next->primary,SECTOR,SECTOR) || fsync(fd) ||
    (mbr_change && (io_write(fd,new_mbr,SECTOR,0) || fsync(fd)))) goto rollback;
 struct gpt check;
 if(load_gpt(fd,&check) || !check.backup_ok ||
    memcmp(check.table,next->table,TABLE_SIZE)) goto rollback;
 return 0;
rollback:
 /* Do not reboot after an unsuccessful transaction. */
 (void)io_write(fd,old->table,TABLE_SIZE,2*SECTOR);
 (void)io_write(fd,old->primary,SECTOR,SECTOR);
 if(mbr_change) (void)io_write(fd,old_mbr,SECTOR,0);
 if(old->backup_ok) {
  (void)io_write(fd,old->table,TABLE_SIZE,(old->backup_lba-32)*SECTOR);
  (void)io_write(fd,old->backup,SECTOR,old->backup_lba*SECTOR);
 }
 (void)fsync(fd); return -1;
}
static int save_backup(int fd,const struct gpt *g) {
#ifndef MYOS_GPT_TEST
 if(mkdir("/overlay/myos-recovery",0700) && errno!=EEXIST) return -1;
 int out=open("/overlay/myos-recovery/gpt-before-stock.bin",
              O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
 if(out<0) return -1;
 unsigned char primary[34*SECTOR], backup[33*SECTOR];
 int ok=io_read(fd,primary,sizeof(primary),0) ||
        io_read(fd,backup,sizeof(backup),(g->backup_lba-32)*SECTOR);
 if(!ok) ok=write(out,primary,sizeof(primary))!=(ssize_t)sizeof(primary) ||
             write(out,backup,sizeof(backup))!=(ssize_t)sizeof(backup) || fsync(out);
 close(out);
 int dir=open("/overlay/myos-recovery",O_RDONLY|O_DIRECTORY);
 if(dir<0) return -1;
 if(fsync(dir)) ok=1;
 close(dir); return ok?-1:0;
#else
 (void)fd; (void)g; return 0;
#endif
}
static int error(const char *s) {
 printf("{\"ok\":false,\"ready\":false,\"error\":\"%s\"}\n",s); return 1;
}
int main(int argc,char **argv) {
 if(argc<2 || argc>3) return error("invalid_action");
 const char *action=argv[1],*path=argc==3?argv[2]:SD_DEVICE;
 int write_mode=strcmp(action,"status")!=0;
 if(strcmp(action,"status") && strcmp(action,"stock") &&
    strcmp(action,"myos") && strcmp(action,"grow")) return error("invalid_action");
#ifndef MYOS_GPT_TEST
 if((!strcmp(action,"stock") || !strcmp(action,"grow")) &&
    (strcmp(path,SD_DEVICE) || !board_sd())) return error("not_n5000_sd");
#endif
 int fd=open(path,(write_mode?O_RDWR:O_RDONLY)|O_NOFOLLOW);
 if(fd<0) return error("cannot_open_sd");
 if(flock(fd,write_mode?LOCK_EX|LOCK_NB:LOCK_SH|LOCK_NB)) {
  close(fd); return error("sd_busy");
 }
 struct gpt g;
 if(load_gpt(fd,&g)) {close(fd);return error("unexpected_ultra_gpt");}
 int native_mode=g.native?native_boot_mode():-1;
 int stock=g.native?native_mode==0:named(g.table+128,STOCK_NAME);
 if(!strcmp(action,"status")) {
  struct statvfs fs; uint64_t bytes=0,avail=0;
  struct native_root root={.fstype="unknown"};
  if(g.native && native_root_info(&root)) {
   memset(&root,0,sizeof(root)); root.fstype="unknown";
  }
  int overlay=mounted_overlay(g.native);
  if(overlay && !statvfs("/overlay",&fs)) {
   bytes=(uint64_t)fs.f_blocks*fs.f_frsize;
   avail=(uint64_t)fs.f_bavail*fs.f_frsize;
  }
  uint64_t p7=g.p7?(u64(g.table+6*128+40)-u64(g.table+6*128+32)+1)*SECTOR:0;
  uint64_t data=g.native?(u64(g.table+3*128+40)-u64(g.table+3*128+32)+1)*SECTOR:p7;
  printf("{\"ok\":true,\"ready\":%s,\"mode\":\"%s\",\"sd_bytes\":%llu,"
         "\"layout\":\"%s\",\"data_bytes\":%llu,"
         "\"p7_bytes\":%llu,\"overlay_bytes\":%llu,\"available_bytes\":%llu,"
         "\"rootfs_data_offset\":%llu,\"rootfs_data_bytes\":%llu,"
         "\"filesystem_bytes\":%llu,\"filesystem\":\"%s\","
         "\"backup_gpt_valid\":%s}\n",
         board_sd()&&g.backup_ok&&overlay&&(g.native?native_mode==1:g.p7)?"true":"false",
         g.native&&native_mode<0?"unknown":stock?"stock":"myos",(unsigned long long)(g.sectors*SECTOR),
         g.native?"official":"legacy",(unsigned long long)data,
         (unsigned long long)p7,(unsigned long long)bytes,
         (unsigned long long)avail,(unsigned long long)root.offset,
         (unsigned long long)root.data_bytes,(unsigned long long)root.fs_bytes,
         root.fstype,g.backup_ok?"true":"false");
  close(fd); return 0;
 }
 /* Native boot selection stays in SPI. Only the final p4 may grow. */
 if(g.native && strcmp(action,"grow")) {close(fd);return error("native_layout_read_only");}
 if(!g.backup_ok) {close(fd);return error("backup_gpt_invalid");}
 if(g.native) {
  struct native_root root;
  if(!native_running_root() || native_root_info(&root)) {
   close(fd);return error("native_root_not_active");
  }
  if(u64(g.table+3*128+40)!=u64(g.primary+48)) {
   close(fd);return error("native_tail_reserved");
  }
 }
 if((!strcmp(action,"stock") && stock) || (!strcmp(action,"myos") && !stock)) {
  close(fd);puts("{\"ok\":true,\"changed\":false}");return 0;
 }
 struct gpt next=g;
 if(!strcmp(action,"stock")) {
  if(!g.p7 || !mounted_overlay(0)) {close(fd);return error("overlay_not_ready");}
  if(save_backup(fd,&g)) {close(fd);return error("backup_failed");}
  set_name(next.table+128,STOCK_NAME);
 } else if(!strcmp(action,"myos")) set_name(next.table+128,"u-boot-env");
 else {
  if(!g.native && (!g.p7 || stock)) {close(fd);return error("p7_not_ready");}
  int index=g.native?3:6;
  uint64_t end=g.sectors-34;
  if(u64(g.table+index*128+40)==end && g.backup_lba==g.sectors-1) {
   close(fd);puts("{\"ok\":true,\"changed\":false}");return 0;
  }
  put64(next.table+index*128+40,end);
 }
 if(commit(fd,&g,&next)) {close(fd);return error("gpt_write_failed");}
 if(!strcmp(action,"grow")) {
#ifndef MYOS_GPT_TEST
  struct blkpg_partition part={0};
  int index=g.native?3:6;
  part.pno=index+1;
  part.start=(long long)(u64(next.table+index*128+32)*SECTOR);
  part.length=(long long)((u64(next.table+index*128+40)-u64(next.table+index*128+32)+1)*SECTOR);
  struct blkpg_ioctl_arg arg={BLKPG_RESIZE_PARTITION,0,sizeof(part),&part};
  /* A busy kernel may defer the new partition length until next boot. */
  (void)ioctl(fd,BLKPG,&arg);
#endif
 }
 close(fd);
 printf("{\"ok\":true,\"changed\":true,\"mode\":\"%s\"}\n",
        !strcmp(action,"stock")?"stock":!strcmp(action,"myos")?"myos":"unchanged");
 return 0;
}
