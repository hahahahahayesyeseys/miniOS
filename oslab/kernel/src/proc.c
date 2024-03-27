#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM];
static proc_t *curr = &pcb[0];

void init_proc() {
  // Lab2-1, set status and pgdir
  // Lab2-4, init zombie_sem
  // Lab3-2, set cwd
pcb[0].status=RUNNING;
pcb[0].pgdir=vm_curr();
pcb[0].cwd=iopen("/",TYPE_NONE);
sem_init(&pcb[0].zombie_sem,0);
}

proc_t *proc_alloc() {
  Log("proc_alloc\n");
  // Lab2-1: find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
  // init ALL attributes of the pcb
 //  printf("proc_alloc\n");
  for(int i=1;i<PROC_NUM;i++){
    if(pcb[i].status==UNUSED){
      pcb[i].pid=next_pid;
      next_pid++;
      pcb[i].brk=0;
      pcb[i].status=UNINIT;
      pcb[i].pgdir=vm_alloc();
      pcb[i].kstack=kalloc();
     // *pcb[i].ctx=pcb[i].kstack->ctx;
      pcb[i].ctx=&pcb[i].kstack->ctx;
      pcb[i].child_num=0;
      pcb[i].parent=NULL;
      pcb[i].cwd=NULL;
      sem_init(&pcb[i].zombie_sem,0);
      for(int j=0;j<MAX_USEM;++j){
        pcb[i].usems[j]=NULL;
      }
      for(int k=0;k<MAX_UFILE;++k){
        pcb[i].files[k]=NULL;
      }
       return &pcb[i];
    }
  }
  return NULL;
 //  printf("proc_alloc failed\n");
}

void proc_free(proc_t *proc) {
  // Lab2-1: free proc's pgdir and kstack and mark it UNUSED
 //  printf("free: %x\n",proc->pid);
    
   proc->status=UNUSED;
}

proc_t *proc_curr() {
  return curr;
}

void proc_run(proc_t *proc) {
  proc->status = RUNNING;
  curr = proc;
  set_cr3(proc->pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
  irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc) {
  // Lab2-1: mark proc READY
   //printf(" proc_addready\n");
  proc->status = READY;
}

void proc_yield() {
  // Lab2-1: mark curr proc READY, then int $0x81
  
  curr->status = READY;
  INT(0x81);
  
}

void proc_copycurr(proc_t *proc) {
  Log("proc_copycurr\n");
  // Lab2-2: copy curr proc
  // Lab2-5: dup opened usems
  // Lab3-1: dup opened files
  // Lab3-2: dup cwd
  //printf("proc_copycurr\n");
  vm_copycurr(proc->pgdir);
  proc->brk=proc_curr()->brk;
  proc->kstack->ctx=proc_curr()->kstack->ctx;
  proc->ctx->eax=0;
  proc->parent=proc_curr();
  proc_curr()->child_num++;
  proc->cwd=idup(proc_curr()->cwd);
 // 复制usems
  for(int i = 0; i < MAX_USEM; i++) {
    proc->usems[i] = proc_curr()->usems[i];
    if(proc->usems[i] != NULL) {
      // 如果usem被复制到了子进程，增加其引用计数
      usem_dup(proc->usems[i]);
    }
  }
  for(int j=0;j<MAX_UFILE;++j){
    proc->files[j]=proc_curr()->files[j];
    if(proc->files[j]!=NULL){
      //printf("proc_copycurr\n copy:%d",j);
      fdup(proc->files[j]);
  }
}
  

}

void proc_makezombie(proc_t *proc, int exitcode) {
  Log("proc_makezombie\n");
  // Lab2-3: mark proc ZOMBIE and record exitcode, set children's parent to NULL
  // Lab2-5: close opened usem
  // Lab3-1: close opened files
  // Lab3-2: close cwd
   //printf(" proc_makezombie\n");
   proc->status=ZOMBIE;
   proc->exit_code=exitcode;
   for(int i=0;i<PROC_NUM;++i){
      if(pcb[i].parent!=NULL){
          if(pcb[i].parent==proc ){
            pcb[i].parent=NULL;
          }
      }
   }
  // 释放usems并调用usem_close
  for(int i = 0; i < MAX_USEM; i++) {
    if(proc->usems[i] != NULL) {
      usem_close(proc->usems[i]);
      proc->usems[i] = NULL;
    }
  }

for(int j=0;j<MAX_UFILE;++j){
  if(proc->files[j]!=NULL){
    fclose(proc->files[j]);
    proc->files[j]=NULL;
  }
}

   if(proc->parent!=NULL){
    sem_v(&proc->parent->zombie_sem);
   }

   iclose(proc->cwd);
}

proc_t *proc_findzombie(proc_t *proc) {
 
  // Lab2-3: find a ZOMBIE whose parent is proc, return NULL if none
   for(int i=0;i<PROC_NUM;++i){
      if(pcb[i].parent!=NULL){
        if(pcb[i].parent==proc&&pcb[i].status==ZOMBIE){
          return &pcb[i];
        }
      }
   }
   return NULL;
}

void proc_block() {
  // Lab2-4: mark curr proc BLOCKED, then int $0x81
  curr->status = BLOCKED;
  INT(0x81);
}

int proc_allocusem(proc_t *proc) {
  // Lab2-5: find a free slot in proc->usems, return its index, or -1 if none
   for(int i = 0; i < MAX_USEM; i++) {
    if(proc->usems[i] == NULL) {
       return i;
    }
  }
  return -1;

}

usem_t *proc_getusem(proc_t *proc, int sem_id) {
  // Lab2-5: return proc->usems[sem_id], or NULL if sem_id out of bound
  
  if(sem_id<0||sem_id>=MAX_USEM){
    return NULL;
  }
  return proc->usems[sem_id];
}

int proc_allocfile(proc_t *proc) {

  // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
  for(int i=0;i<MAX_UFILE;++i){
    if(proc->files[i]==NULL ){
        Log("proc_allocfile id:%d\n",i);
      return i;
    }
  }
  return -1;
}

file_t *proc_getfile(proc_t *proc, int fd) {
  Log("proc_getfile ,fd:%d\n",fd);
  // Lab3-1: return proc->files[fd], or NULL if fd out of bound
  if(fd<0||fd>=MAX_UFILE){
    return NULL;
  }
  return proc->files[fd];
}

void schedule(Context *ctx) {
  // Lab2-1: save ctx to curr->ctx, then find a READY proc and run it
  
   proc_curr()->ctx=ctx;
   int pid_cur=proc_curr()->pid;
   int tmp=0;
   //bug在这里，pid并不是它自己的位置。。。
   for(int i=0;i<PROC_NUM;++i){
    if(pcb[i].pid==pid_cur){
        tmp=i;
        break;
    }

   }
   for(int i=(tmp+1)%PROC_NUM;;i=(i+1)%PROC_NUM){
    if(pcb[i].status==READY){
      proc_run(&pcb[i]);
    }
   }
}

/*
 
void schedule(Context *ctx) {
  // Save current context to curr->ctx
  curr->ctx = ctx;

  // Find a READY process to run
  for (int i = (curr - pcb + 1) % PROC_NUM; i != (curr - pcb) % PROC_NUM; i = (i + 1) % PROC_NUM) {
    if (pcb[i].status == READY) {
      proc_run(&pcb[i]);
      return;
    }
  }

  // If no READY process is found, continue running the current process
  proc_run(curr);
}

*/
