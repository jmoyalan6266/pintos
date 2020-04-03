#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/filesys.h"


static void syscall_handler (struct intr_frame *);
void halt (void);
tid_t exec (const char *cmd_line);
int write (int fd, const void *buffer, unsigned size);
void exit (int status);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
int read (int fd, void *buffer, unsigned size); 
int open (const char *file);
int filesize (int fd);
struct lock lock;

// helper
bool check_valid(char* cmd_line);

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
  int fd;
  char* file;
  void* buffer;
  unsigned size;
  if(!check_valid(myEsp))
  {
    exit(-1);
  }
  switch(*myEsp)
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT: 
      myEsp++;
      int status = *myEsp;
      if(!check_valid(myEsp))
      {
        exit(-1);
      }
      exit (status);
      break;
    case SYS_EXEC:
      myEsp++;
      char *cmd_line = (char *)*myEsp;
      f->eax = exec(cmd_line);
      break;
    case SYS_WAIT:  
      myEsp++;
      tid_t pid = *myEsp;
      f->eax = wait (pid);
      break;
    case SYS_CREATE:
      myEsp++;
      file = (char *)*myEsp;
      myEsp++;
      unsigned initial_size = *myEsp;
      f->eax = create(file, initial_size);
      break;
    case SYS_REMOVE:
      myEsp++;
      file = (char *)*myEsp;
      f->eax = remove(file);
      break;
    case SYS_OPEN:
      myEsp++;
      file = (char*)*myEsp;
      f->eax = open (file);
      break;
    case SYS_FILESIZE:
      myEsp++;
      fd = *myEsp;
      f->eax = filesize(fd);
      break;
    case SYS_READ:
      myEsp++;
      fd = *myEsp;
      myEsp++;
      buffer = (void*)*myEsp;
      myEsp++;
      size = *myEsp;
      f->eax = read (fd, buffer, size);
      break;
    case SYS_WRITE:
      myEsp++;
      fd = *myEsp;
      myEsp++;
      buffer = (void*)*myEsp;
      myEsp++;
      size = *myEsp;
      f->eax = write (fd, buffer, size);
      break;
    case SYS_SEEK:
      myEsp++;
      fd = *myEsp;
      myEsp++;
      unsigned position = *myEsp;
      seek(fd, position);
      break;
    case SYS_TELL:
      myEsp++;
      fd = *myEsp;
      f->eax = tell(fd);
      break;
    case SYS_CLOSE:
      myEsp++;
      fd = *myEsp;
      close(fd);
      break;
  }
}

void 
halt (void)
{
  shutdown_power_off();
}

void
exit (int status)
{
  struct thread *curr = thread_current();
  curr->exit = status;
  char* save_ptr;
  char* name_cpy = strtok_r (curr->name, " ", &save_ptr);
  printf ("%s: exit(%d)\n", name_cpy, status);
  thread_exit();
}

//pid_t -> why not this
tid_t 
exec (const char *cmd_line)
{
  if (!check_valid(cmd_line)) 
  {
    return -1;
  }
  tid_t tid = process_execute(cmd_line);
  struct thread *curr = thread_current();
  sema_down(&curr->le_sema);
  if (!curr->le_pass)
  {
    return -1;
  }
  if(tid == TID_ERROR)
  {
    return -1;
  }
  return tid;
}

int
wait (tid_t pid)
{
  int ret = process_wait(pid);
  return ret;
}

bool
create (const char *file, unsigned initial_size)
{
  if (!check_valid(file)) 
  {
    exit(-1);
  }
  bool returnVal;
  lock_acquire(&lock);
  returnVal = filesys_create(file, initial_size);
  lock_release(&lock);
  return returnVal;
}

bool
remove (const char *file)
{
  if (!check_valid(file)) 
  {
    return -1;
  }
  bool returnVal;
  lock_acquire(&lock);
  returnVal = filesys_remove(file);
  lock_release(&lock);
  return returnVal;
}

int
open (const char *file)
{
  if (!check_valid(file)) 
  {
    exit(-1);
  }
  bool notFound = 1;
  int index = 2;
  lock_acquire(&lock);
  struct file *fp = filesys_open(file);
  lock_release(&lock);
  struct thread *curr = thread_current();
  if (fp == NULL)
  {
    return -1;
  }
  else
  {
    while (notFound && index < 128)
    {
      if (curr->fileDir[index] != NULL)
      {
        index++;
      }
      else
      {
        notFound = 0;
        curr->fileDir[index] = fp;
      }
    }
  }
  if (notFound)
  {
    return -1;
  }
  return index;
}

int
filesize (int fd)
{
  if (fd < 2 || fd > 127)
  {
    return -1;
  }
  
  struct thread *curr = thread_current();
  struct file *file = curr->fileDir[fd];
  if (file == NULL)
  {
    return -1;
  }
  lock_acquire(&lock);
  int retVal = (int)file_length(file);
  lock_release(&lock);
  return retVal;
}

int
read (int fd, void* buffer, unsigned size)
{
  if (!check_valid(buffer) || (fd < 0 || fd > 127)) 
  {
    exit(-1);
  }
  int noBytes = 0;
  
  struct thread *curr = thread_current();
  int i;
  if(fd == 0)
  {
    for (i = 0; i < size; i++)
    {
      //TODO: what to do with input_getc()
      input_getc();
      noBytes++;
    }
  }
  else if (fd == 1)
  {
    return -1;
  }
  else 
  {  
     struct file *file = curr->fileDir[fd];
     lock_acquire(&lock);
     noBytes = (int)file_read(file, buffer, size);
     lock_release(&lock);
  }
  
  return noBytes;
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (!check_valid(buffer) || (fd < 0 || fd > 127)) 
  {
    exit(-1);
  }
  int noBytes;
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
  else if(fd == 0)
  {
    return -1;
  }
  else 
  {
     noBytes = 0;
     struct file *file = curr->fileDir[fd];
     lock_acquire(&lock);
     noBytes = (int)file_write(file, buffer, size);
     lock_release(&lock);
  }
  return noBytes;
}

void
seek (int fd, unsigned position)
{
  if (fd < 0 || fd > 127)
  {
    return;
  }
  
  struct thread *curr = thread_current(); 
  struct file *file = curr->fileDir[fd];
  if (file == NULL)
  {
    return;
  }
  lock_acquire(&lock);
  file_seek(file, position);
  lock_release(&lock);
}

unsigned
tell (int fd)
{
  if (fd < 0 || fd > 127)
  {
    return -1;
  }
  struct thread *curr = thread_current(); 
  struct file *file = curr->fileDir[fd];
  if (file == NULL)
  {
    return -1;
  }
  lock_acquire(&lock);
  unsigned ret = file_tell(file);
  lock_release(&lock);
  return ret;
}

void 
close (int fd)
{
  //checks for valid fd
  if (fd < 2 || fd > 127)
  {
    return;
  }
  struct thread *curr = thread_current();
  struct file *file = curr->fileDir[fd];
  if (file == NULL)
  {
    return;
  }
  lock_acquire(&lock);
  file_close(file);
  curr->fileDir[fd] = NULL;
  lock_release(&lock);
}

bool
check_valid(char* cmd_line)
{
  struct thread *curr = thread_current();
  if (cmd_line == NULL || !is_user_vaddr(cmd_line))
  {
    return false;
  } 
  else if (pagedir_get_page (curr->pagedir, cmd_line) == NULL)
  {
    return false;
  }
  return true;
}
