#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/off_t.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
void halt (void);
tid_t exec (const char *cmd_line);
int write (int fd, const void *buffer, unsigned size);
void exit (int status);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
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
  switch(*myEsp)
  {
    case SYS_HALT:
      halt();
      break;
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
      void* buffer = (void*)*myEsp;
      myEsp++;
      unsigned size = *myEsp;
      f->eax = write (fd, buffer, size);
      break;
    case SYS_EXEC:
      myEsp++;
      const char *cmd_line = *myEsp;
      f->eax = exec(cmd_line);
      break;
    case SYS_CREATE:
      myEsp++;
      const char *file = *myEsp;
      myEsp++;
      unsigned initial_size = *myEsp;
      f->eax = create(file, initial_size);
      break;
    case SYS_REMOVE:
      myEsp++;
      const char *file1 = *myEsp;
      f->eax = remove(file1);
      break;

  }
  thread_exit ();
}

int
wait (tid_t pid)
{
  int ret = process_wait(pid);
  return ret;
}

void
exit (int status)
{
  struct thread *curr = thread_current();
  curr->exit = status;
  thread_exit();
}

int
write (int fd, const void *buffer, unsigned size)
{
  int noBytes;
  //checks to see if it is a valid address
  if (!is_user_vaddr(buffer) || (fd < 0 || fd > 127))
  {
    exit(-1);
  }
  lock_acquire(&lock);
  struct thread *curr = thread_current();
  if(fd == 1)
  {
    while(size >= 128)
    {
      putbuf((char*)buffer, 128);
      size -= 128;
      buffer += 128;
    }
    if(size > 0)
    {
      putbuf((char*)buffer, size);
    }
    noBytes = size;
  }
  else 
  {
     noBytes = 0;
     struct file *file = curr->fileDir[fd];
     noBytes = (int)file_write(file, buffer, size);
  }
  lock_release(&lock);
  return noBytes;
}

//pid_t -> why not this
tid_t 
exec (const char *cmd_line)
{
  lock_acquire(&lock);
  tid_t tid = process_execute(cmd_line);
  if(tid == TID_ERROR)
  {
    lock_release(&lock);
    //change to macro
    return -1;
  }
  lock_release(&lock);
}

void 
halt (void)
{
  shutdown_power_off();
}

bool
create (const char *file, unsigned initial_size)
{
  bool returnVal;
  lock_acquire(&lock);
  returnVal = filesys_create(file, initial_size);
  lock_release(&lock);
  return returnVal;
}

bool
remove (const char *file)
{
  bool returnVal;
  lock_acquire(&lock);
  returnVal = filesys_remove(file);
  lock_release(&lock);
  return returnVal;
}