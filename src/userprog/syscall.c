#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);
struct lock lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  //find out how to get syscall_no
  int* myEsp = f->esp;
  printf(*myEsp);
  switch(*myEsp)
  {
    case SYS_EXIT: 
      myEsp++;
      int status = *myEsp;
      exit (status);
      break;
    case SYS_WAIT:  
      myEsp++;
      tid_t pid = *myEsp;
      f->eax = wait (pid);
      break;
    case SYS_WRITE:
      myEsp++;
      int fd = *myEsp;
      myEsp++;
      void* buffer = *myEsp;
      myEsp++;
      unsigned size = *myEsp;
      f->eax = write (fd, buffer, size);
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
      struct thread *temp = list_entry (e, struct thread, c_elem);
      sema_up(&(temp->c_sema2));
      list_remove(e);
      //free all of threads current children
    }
  process_exit();
}

int
write (int fd, const void *buffer, unsigned size)
{
  putbuf(buffer, size);
  // int noBytes;
  // //checks to see if it is a valid address
  // if (!is_user_vaddr(buffer) || (fd < 0 || fd > 127))
  // {
  //   exit(-1);
  // }
  // lock_acquire(lock);
  // struct thread *curr = thread_current();
  // if(fd == 1)
  // {
  //   putbuf((char*)buffer, size);
  //   noBytes = size;
  // }
  // else 
  // {
  //    struct file *file = curr->fileDir[fd];
  //    noBytes = file_write(file, buffer, size);
  // }
 
  // lock_release(lock);
  // return (int)noBytes;
}