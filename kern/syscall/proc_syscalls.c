#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <machine/trapframe.h>
#include <synch.h>
#include <limits.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include "opt-A2.h"
//Howe Yang: Student id 20472168




//need to make a sys fork
//#if OPT_A2
//global variable
//static int globalpid = 10;
//static struct lock * globallock;


int sys_fork(struct trapframe* tf,pid_t *retval ){
//

//make proc strucutre====================================

struct proc * procbase;
procbase = proc_create_runprogram(curproc->p_name); //should i have a base name? or variable counter?
//ASSERT
KASSERT(procbase != NULL);

//create and copy adrress space====================================
//1. make a new address space
//2. copy the address space from the current thread to the new one
//3. set the new address space as the address space for the new proc.

struct addrspace * as;
int success;

success = as_copy(curproc->p_addrspace, &as);

//check for error
if(success){
   //kprintf("rest in pepperonis.");
   proc_destroy(procbase);
   return ENOMEM;
   }
//make sure u acquire lock and set parent pid

spinlock_acquire(&procbase->p_lock);
procbase->p_addrspace = as;
procbase->parent_pid = curproc->pid;
spinlock_release(&procbase->p_lock);
//address copied and uniquely set.
//record the parent to the process tree

lock_acquire(globallock);
globalparent[procbase->pid] = curproc->pid;
lock_release(globallock);


//incrementing and setting pid... not very cool==================
//note* i need to get a lock/cv on this global variable later~.
//Using the lock from proc.h / proc.c and the static variable there :D
//lock_acquire(globallock);
//kprintf("this guy has pid of :" + globalpid);
/*
procbase->pid = globalpid;
globalpid = globalpid + 1;
lock_release(globallock);
*/

//globalpid = globalpid + 1;



//make trap frame using mips_trap============================?
//--- do... i grab the stack frame?
// make a dummy trap frame on stack?
struct trapframe * riptf;

riptf = kmalloc(sizeof(struct trapframe));

*riptf = *tf;
//looks looks right..
	

//forking? set the the return function to trap frame :D?
//kprintf("made it past trap frame \n");

int forker;

forker = thread_fork(curthread->t_name, procbase, (void *)enter_forked_process, riptf,0);

//avoid complaints
//kprintf("made it past forker \n");
//double return??
if(forker){
   //kprintf("TERRIBLE THINGS HAVE OCCURED \n");
   as_destroy(procbase->p_addrspace);
   proc_destroy(procbase);
   kfree(riptf);
   return forker;
}

   *retval = procbase->pid;

return 0;
}
//#endif /* OPT_A2 */

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);
  KASSERT(curproc->p_addrspace != NULL);
   as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);
  
  
  //lets do something ====================
  lock_acquire(globallock);

   if(globalfinish[p->parent_pid] != 1){
   globalfinish[p->pid] = 1;
  globalexit[p->pid] = _MKWAIT_EXIT(exitcode);
  cv_broadcast(globalcv,globallock);
  }else{
  globalfinish[p->pid] = 1;
   globalparent[p->pid] = -1;
   globalexit[p->pid] = _MKWAIT_EXIT(exitcode);
   globalcurrent[p->pid] = 0;
  }
  
  for(int counter = 0; counter < 3000; counter ++){
  if(globalparent[counter] == p->pid){
  globalparent[counter] = -1;
  globalcurrent[counter] = 0;
  }
  
  }
  
  lock_release(globallock);
  
  //end of addition============================
  
  proc_remthread(curthread);
  proc_destroy(p);
  thread_exit();
  panic("return from thread_exit in sys_exit\n");
}


int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}


int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  if (options != 0) {
    return(EINVAL);
  }
  //exitstatus = 0;
  
  //Alright lets fix it.
  //make sure the parent is the one calling it
  
  if(globalparent[pid] != curproc->pid){
   return(ECHILD);
  }
  // while globalfinish = 0 cvwait...lets work on exit
  
  lock_acquire(globallock);
  while(globalfinish[pid] == 0){
  
  cv_wait(globalcv, globallock);
  
  }
  exitstatus = globalexit[pid];
  
  
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    lock_release(globallock);
    return(result);
  }
  
  globalcurrent[pid] = 0;
  *retval = pid;
  lock_release(globallock);
  
  return(0);
}

//////////////////////
//A2B PLS GOD.


int 
sys_execv(userptr_t progname, userptr_t args) {

//nsdiv~
  int result;
  struct vnode *v;
  vaddr_t stackptr;
  
//BASIC CHECKS
if(progname == NULL || (char**)args == NULL){
return ENOENT;
}
//copy progname
  char *progname_temp = kmalloc(PATH_MAX);
  size_t prog_length;
  result = copyinstr(progname, progname_temp, PATH_MAX, &prog_length);
  if(result != 0){
  return ENOMEM;
  }

  //Figure out size of the arguments?
  //variables
  
  char *buffer;
  char *argrip;
  int argc = 0;
  size_t arglengths[argc];
  char **argptrs = kmalloc(sizeof(char *) * ARG_MAX);
  //y0l000000000000000000000000000000000000000000000000000
  //getting length of argc?
  while(1) {
    argrip = NULL;
    result = copyin(args + sizeof(char *) * argc, &argrip, sizeof(char *));
    
    if(result == 1) {
      return result;
    }
    if(argrip != NULL) {
    result = copyin(args + sizeof(char *) * argc, &argptrs[argc], sizeof(char *));
    
    if(result == 1) {
      return result;
    }
    ///========================
    buffer = argptrs[argc];
    
    argptrs[argc] = kmalloc(ARG_MAX);
    result = copyinstr((userptr_t) argrip, argptrs[argc], ARG_MAX, &arglengths[argc]);
    
    if(result == 1) {
      return result;
    }
    //=========================
      argc++;
    }else{
    break;
    }
  }
  //DONEEE
  
  //open file
  result = vfs_open(progname_temp, O_RDONLY, 0, &v);
  if (result != 0) {
    return result;
  }


  // address space
  //variables
  
   struct addrspace *as;
   vaddr_t entrypoint;
   
   //work
  as = as_create();
  if (as == NULL) {
    vfs_close(v);
    return ENOMEM;
  }
  
  // switchhhhhhhhhhhh might need to fix later
  curproc_setas(as);
  as_activate();
  result = load_elf(v, &entrypoint);
  if (result) {
    vfs_close(v);
    return result;
  }

  // close file
  vfs_close(v);
  

  ///addresss space define
  result = as_define_stack(as, &stackptr);
  //hhmmm
  ///argument strings to userspace
  //pad
   stackptr -= sizeof(userptr_t) * (argc+1);
   stackptr -= sizeof(char *);
  
  if (result) {
    return result;
  }

  
  //variable
  int indexer = 0;
  //set to somewhere
  while(indexer < argc){
    stackptr -= arglengths[indexer];
    
    result = copyoutstr(argptrs[indexer], (userptr_t) stackptr, arglengths[indexer], NULL);
   
    if(result != 0) {
      return result;
    }
    indexer = indexer + 1;
  }

  //arguments pointers to userspace!
  indexer = argc -1;
  
  while(indexer >= 0){
  //freeee
    stackptr -= sizeof(char *);
    kfree(argptrs[indexer]);
    //settt
    argptrs[indexer] = (char *) stackptr;
    result = copyout(&argptrs[indexer], (userptr_t) stackptr, sizeof(char *));
    
    if(result !=0 ){
      return result;
    }
    indexer = indexer - 1;
  }

  ///argument pointerrrrr?
  char **argspoint = (char **) stackptr;

   //FREE up memory here?
  kfree(progname_temp);
  kfree(argptrs);
 
  
  //exit out~
  enter_new_process(argc, (userptr_t) argspoint, stackptr, entrypoint);
  
  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
  
}



///addition

void murder_thread(int exitcode){
//varaibles

lock_acquire(globallock);

struct addrspace *as;
struct proc *p = curproc;


//reuse pid
if(globalfinish[p->parent_pid] != 1){
   globalfinish[p->pid] = 1;
  globalexit[p->pid] = _MKWAIT_EXIT(exitcode);
  cv_signal(globalcv,globallock);
  }else{
  globalfinish[p->pid] = 1;
   globalparent[p->pid] = -1;
   globalexit[p->pid] = _MKWAIT_SIG(exitcode);
   globalcurrent[p->pid] = 0;
  }
  
as_deactivate();  

as = curproc_setas(NULL);
as_destroy(as);
//rem thread? not necessary
proc_remthread(curthread);
//closing proc...
proc_destroy(p);

lock_release(globallock);
thread_exit();

panic("MOTHERFUCKER");

}
