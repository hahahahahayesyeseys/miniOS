#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx) {
 
  int sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS) {
    res = -1;
  } else {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count) {
   Log("sys_write\n");
 
  file_t* file=proc_getfile(proc_curr(),fd);
  if(file==NULL){
    return -1;
  }
  int ret=fwrite(file,buf,count);
  return ret;
}

int sys_read(int fd, void *buf, size_t count) {
  Log("sys_read,fd:%d,count:%d\n",fd,count);
 
  file_t* file=proc_getfile(proc_curr(),fd);
  if (file == NULL) {
    return -1;
  }
  int ret = fread(file, buf, count);
  return ret;
}

int sys_brk(void *addr) {
     // printf("sys_brk\n");
 
  //static size_t brk = 0; // use brk of proc instead of this in Lab2-1
  size_t new_brk = PAGE_UP(addr);
  if (proc_curr()->brk == 0) {
    proc_curr()->brk  = new_brk;
  } else if (new_brk > proc_curr()->brk ) {
    PD* pdCur=vm_curr();
    vm_map(pdCur,proc_curr()->brk ,new_brk-proc_curr()->brk ,7);
    proc_curr()->brk  = new_brk;

  } else if (new_brk < proc_curr()->brk ) {
    // can just do nothing
  }
  
  return 0;
}

void sys_sleep(int ticks) {
   // printf("sys_sleep\n");
   uint32_t start = get_tick();  // 获取当前系统时间
    while (get_tick() - start < ticks) {  // 判断是否已经等待足够的时间
        // sti();  // 开中断
        // hlt();  // 进入睡眠状态
        // cli();  // 关中断
        proc_yield();
    }
 
}

int sys_exec(const char *path, char *const argv[]) {
 
  PD* new_pgdir=vm_alloc();
  Context ctx;
  int load_ret=load_user(new_pgdir,&ctx,path,argv);
  if(load_ret!=0){
    //vm_free(pgdir);？
    printf("sys_exec:ret==-1");
    return -1;
  }
  //PD* old_pgdir=vm_curr();
  proc_curr()->pgdir=new_pgdir;
  set_cr3(new_pgdir);
  //  vm_free(old_pgdir);

  
  irq_iret(&ctx);
  //printf("sys_exec\n");

}

int sys_getpid() {
   // Lab2-1
   return proc_curr()->pid;
}

void sys_yield() {
  proc_yield();
}

int sys_fork() {
  //  printf("sys_fork\n");
   // Lab2-2
   proc_t* child=proc_alloc();
   if(child==NULL){
    return -1;
   }
   // printf("parent: %x\n",proc_curr()->pid);
 //   printf("child: %x\n",child->pid);
   proc_copycurr(child);
   proc_addready(child);
 
   return child->pid;
}

void sys_exit(int status) {
 
 //   printf("sys_exit\n");
 //   printf("exit: %x\n",proc_curr()->pid);
   proc_makezombie(proc_curr(),status);
    
  INT(0X81);
  assert(0);

  //while (1) proc_yield();

}

int sys_wait(int *status) {
 // printf("sys_wait\n");
 
  int pid;
  //printf("wait:parent: %x\n",proc_curr()->pid);
  if(proc_curr()->child_num==0) return -1;
  
  sem_p(&proc_curr()->zombie_sem);
  proc_t*child=proc_findzombie(proc_curr());

while(child==NULL){
proc_yield();
child=proc_findzombie(proc_curr());
}
if(status !=NULL){
*status=child->exit_code;
}
pid=child->pid;
//printf("sys_wait:pid: %x\n",pid);
proc_free(child);
proc_curr()->child_num--;

return pid;

// sys_sleep(250);
// return 0;
}

int sys_sem_open(int value) {
  int id=proc_allocusem(proc_curr());
  if(id==-1){
    return -1;
  }
  usem_t* usemm= usem_alloc(value);
  if(usemm==NULL){
    return -1;
  }
  proc_curr()->usems[id]=usemm;
  return id;
}

int sys_sem_p(int sem_id) {
  usem_t* usemm= proc_getusem(proc_curr(),sem_id);
  if(usemm==NULL){
    return -1;
  }
  sem_p(&usemm->sem);
return 0;
}

int sys_sem_v(int sem_id) {
  usem_t* usemm= proc_getusem(proc_curr(),sem_id);
  if(usemm==NULL){
    return -1;
  }
  sem_v(&usemm->sem);
return 0;
}

int sys_sem_close(int sem_id) {
  usem_t* usemm= proc_getusem(proc_curr(),sem_id);
  if(usemm==NULL){
    return -1;
  }
  usem_close(usemm);
  proc_curr()->usems[sem_id]=NULL;
  return 0;

}

int sys_open(const char *path, int mode) {
   Log("sys_open\n");
 int fd = proc_allocfile(proc_curr());
 if(fd==-1){
  return -1;
 }
 file_t* file=fopen(path,mode);
 if(file==NULL){
  return -1;
 }
 proc_curr()->files[fd]=file;
 return fd;
}

int sys_close(int fd) {
   Log("sys_close\n");
  file_t *file = proc_getfile(proc_curr(),fd);
  if (file == NULL) {
    return -1;
  }
  fclose(file);
  proc_curr()->files[fd]=NULL;

 return 0;
}

int sys_dup(int fd) {
   Log("sys_dup\n");
   file_t* old=proc_getfile(proc_curr(),fd);
   if(old==NULL){
    return -1;
   }
   int new_fd=proc_allocfile(proc_curr());
   if(new_fd<0){
    return -1;
   }
  proc_curr()->files[new_fd] = fdup(old);
  return new_fd;
}

uint32_t sys_lseek(int fd, uint32_t off, int whence) {
   Log("sys_lseek\n");
  file_t *file = proc_getfile(proc_curr(),fd);
  if (file == NULL) {
    return -1;
  }
  int ret=fseek(file, off, whence);
  return ret;
}

int sys_fstat(int fd, struct stat *st) {
  file_t *file = proc_getfile(proc_curr(), fd);
  if (!file) return -1;
  int type = TYPE_NONE;
  uint32_t size = 0;
  uint32_t node = 0;

  if (file->type == TYPE_FILE) {
    inode_t *ip = file->inode;
    type = itype(ip);
    size = isize(ip);
    node = ino(ip);
  } else if (file->type == TYPE_DEV) {
    type = TYPE_DEV;
  }

  st->type = type;
  st->size = size;
  st->node = node;

  return 0;
}

int sys_chdir(const char *path) {
  //   
  inode_t* nd=iopen(path,O_RDONLY);
  if(nd==NULL){
    return -1;
  }
  int type=itype(nd);
  if(type!=TYPE_DIR){
    return -1;
  }
  iclose(proc_curr()->cwd);
  proc_curr()->cwd=nd;
  return 0;
}

int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

int sys_clone(void (*entry)(void*), void *stack, void *arg) {
  TODO();
}

int sys_kill(int pid) {
  TODO();
}

int sys_cv_open() {
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id) {
  TODO();
}

int sys_cv_sig(int cv_id) {
  TODO();
}

int sys_cv_sigall(int cv_id) {
  TODO();
}

int sys_cv_close(int cv_id) {
  TODO();
}

int sys_pipe(int fd[2]) {
  TODO();
}

int sys_link(const char *oldpath, const char *newpath) {
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath) {
  TODO();
}

void *syscall_handle[NR_SYS] = {
  [SYS_write] = sys_write,
  [SYS_read] = sys_read,
  [SYS_brk] = sys_brk,
  [SYS_sleep] = sys_sleep,
  [SYS_exec] = sys_exec,
  [SYS_getpid] = sys_getpid,
  [SYS_yield] = sys_yield,
  [SYS_fork] = sys_fork,
  [SYS_exit] = sys_exit,
  [SYS_wait] = sys_wait,
  [SYS_sem_open] = sys_sem_open,
  [SYS_sem_p] = sys_sem_p,
  [SYS_sem_v] = sys_sem_v,
  [SYS_sem_close] = sys_sem_close,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_dup] = sys_dup,
  [SYS_lseek] = sys_lseek,
  [SYS_fstat] = sys_fstat,
  [SYS_chdir] = sys_chdir,
  [SYS_unlink] = sys_unlink,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_clone] = sys_clone,
  [SYS_kill] = sys_kill,
  [SYS_cv_open] = sys_cv_open,
  [SYS_cv_wait] = sys_cv_wait,
  [SYS_cv_sig] = sys_cv_sig,
  [SYS_cv_sigall] = sys_cv_sigall,
  [SYS_cv_close] = sys_cv_close,
  [SYS_pipe] = sys_pipe,
  [SYS_link] = sys_link,
  [SYS_symlink] = sys_symlink};
