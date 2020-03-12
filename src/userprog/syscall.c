#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  printf ("system call!\n");
  thread_exit ();
}

int
wait (pid_t pid)
{
  int ret = process_wait(pid);
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
//  syscall1 (SYS_EXIT, status);
//  NOT_REACHED ();
}

// int
// write (int fd, const void *buffer, unsigned size)
// {
//
// }
