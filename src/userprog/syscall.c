#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);
struct lock *lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  //find out how to get syscall_no
  int syscall_no = f->vec_no;
  switch(syscall_no)
  {
    case SYS_EXIT: 
      break;
    case SYS_WAIT:  
      break;
    case SYS_WRITE:
      break;
  }
  printf ("system call!\n");
  thread_exit ();
}

int
wait (pid_t)
{
  int ret = process_wait(pid_t);
  return ret;
}

void
exit (int status)
{
  struct list_elem *e;
  struct thread *curr = thread_current();
  curr->exit = status;
  //gets parent thread
  //unblocks parent thread
  sema_up(&(curr->c_sema1));
  //need to find a way to check if parent is still alive
  //blocks until parent calls wait()
  sema_down(&(curr->c_sema2));
  for (e = list_begin (&curr->children); e != list_end (&curr->children); e = list_next (e))
    {
      list_remove(e);
      //sema_up(&(e->c_sema2)
      //free all of threads current children
    }
  process_exit();
}

int
write (int fd, const void *buffer, unsigned size)
{
  //checks to see if it is a valid address
  if (!is_user_vaddr(buffer))
  {
    exit(-1);
  }
  lock_acquire(lock);
  struct thread *curr = thread_current();
  if (fd == 0)
  {

  }
  else if(fd == 1)
  {

  }
  struct file *file = curr->fileDir[fd];
  off_t noBytes = file_write(file, buffer, (off_t)size);
  lock_release(lock);
  return (int)noBytes;
}
