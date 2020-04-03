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

// syscall methods
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
// lock to block calls to filesys
struct lock lock;

// helper method to check for valid user address
bool check_valid(char* cmd_line);

// initialize the syscall handler
void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  // stores syscall number
  int* myEsp = f->esp;
  // variables used multiple times throughout syscall_handler
  int fd;
  char* file;
  void* buffer;
  unsigned size;
  // check to see if syscall number is a valid address
  if(!check_valid(myEsp))
  {
    exit(-1);
  }
  // each case increments up the stack to grab any parameters
  switch(*myEsp)
  {
    // Joseph drove here
    case SYS_HALT:
      halt();
      break;
    // Sahithi drove here
    case SYS_EXIT: 
      myEsp++;
      int status = *myEsp;
      // check if status is a valid address
      if(!check_valid(myEsp))
      {
        exit(-1);
      }
      exit (status);
      break;
    // Joseph drove here
    case SYS_EXEC:
      myEsp++;
      char *cmd_line = (char *)*myEsp;
      f->eax = exec(cmd_line);
      break;
    // Pranay drove here
    case SYS_WAIT:  
      myEsp++;
      tid_t pid = *myEsp;
      f->eax = wait (pid);
      break;
    // Ashish drove here
    case SYS_CREATE:
      myEsp++;
      file = (char *)*myEsp;
      myEsp++;
      unsigned initial_size = *myEsp;
      f->eax = create(file, initial_size);
      break;
    // Ashish drove here
    case SYS_REMOVE:
      myEsp++;
      file = (char *)*myEsp;
      f->eax = remove(file);
      break;
    // Pranay drove here
    case SYS_OPEN:
      myEsp++;
      file = (char*)*myEsp;
      f->eax = open (file);
      break;
    // Ashish drove here
    case SYS_FILESIZE:
      myEsp++;
      fd = *myEsp;
      f->eax = filesize(fd);
      break;
    // Sahithi drove here
    case SYS_READ:
      myEsp++;
      fd = *myEsp;
      myEsp++;
      buffer = (void*)*myEsp;
      myEsp++;
      size = *myEsp;
      f->eax = read (fd, buffer, size);
      break;
    // Joseph drove here
    case SYS_WRITE:
      myEsp++;
      fd = *myEsp;
      myEsp++;
      buffer = (void*)*myEsp;
      myEsp++;
      size = *myEsp;
      f->eax = write (fd, buffer, size);
      break;
    // Sahithi drove here
    case SYS_SEEK:
      myEsp++;
      fd = *myEsp;
      myEsp++;
      unsigned position = *myEsp;
      seek(fd, position);
      break;
    // Sahithi drove here
    case SYS_TELL:
      myEsp++;
      fd = *myEsp;
      f->eax = tell(fd);
      break;
    // Ashish drove here
    case SYS_CLOSE:
      myEsp++;
      fd = *myEsp;
      close(fd);
      break;
  }
}

// Joseph drove here
void 
halt (void)
{
  shutdown_power_off();
}

void
exit (int status)
{
  // Sahithi drove here
  struct thread *curr = thread_current();
  curr->exit = status;
  // Ashish drove here
  // make a copy of the thread name and print it with it's exit status
  char* save_ptr;
  char* name_cpy = strtok_r (curr->name, " ", &save_ptr);
  printf ("%s: exit(%d)\n", name_cpy, status);
  thread_exit();
}

tid_t 
exec (const char *cmd_line)
{
  if (!check_valid(cmd_line)) 
  {
    return -1;
  }
  // Joseph and Pranay drove here
  // use semaphore to synchronize load and exec
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
  // Pranay drove here
  int ret = process_wait(pid);
  return ret;
}

bool
create (const char *file, unsigned initial_size)
{
  // Ashish drove here
  if (!check_valid(file)) 
  {
    exit(-1);
  }
  // creates the file and blocks the filesys call with locks
  bool returnVal;
  lock_acquire(&lock);
  returnVal = filesys_create(file, initial_size);
  lock_release(&lock);
  return returnVal;
}

bool
remove (const char *file)
{
  // Ashish drove here
  if (!check_valid(file)) 
  {
    return -1;
  }
  // removes the file and blocks the filesys call with locks
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
  // Pranay drove here
  // variables to search for file to open
  bool notFound = 1;
  int index = 2;
  // opens the file and blocks the filesys call with locks
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
    // Joseph drove here
    // keep looking through the file directory until next empty 
    // space is found and put the file there
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
  // if no space is found, return -1
  if (notFound)
  {
    return -1;
  }
  return index;
}

int
filesize (int fd)
{
  // Ashish drove here
  if (fd < 2 || fd > 127)
  {
    return -1;
  }
  // returns the size of the file as long as it is not null
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
  // Sahithi drove here
  if (!check_valid(buffer) || (fd < 0 || fd > 127)) 
  {
    exit(-1);
  }
  // variable to count number of bytes
  int noBytes = 0;
  struct thread *curr = thread_current();
  // index variable
  int i;
  // for std_in, return the number of bytes read
  if(fd == 0)
  {
    for (i = 0; i < size; i++)
    {
      input_getc();
      noBytes++;
    }
  }
  // for std_out, return -1
  else if (fd == 1)
  {
    return -1;
  }
  else 
  {  
    // otherwise, call file_read to get number of bytes
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
  // Joseph drove here
  if (!check_valid(buffer) || (fd < 0 || fd > 127)) 
  {
    exit(-1);
  }
  // variable to count number of bytes written
  int noBytes;
  struct thread *curr = thread_current();
  // for std_out, write 128 bytes at a time
  if(fd == 1)
  {
    while(size >= 128)
    {
      // Pranay drove here
      // print 128 to console
      putbuf((char*)buffer, 128);
      size -= 128;
      buffer += 128;
    }
    if(size > 0)
    {
      // print rest to console
      // Pranay drove here
      putbuf((char*)buffer, size);
    }
    noBytes = size;
  }
  // if std_in, return -1
  else if(fd == 0)
  {
    return -1;
  }
  else 
  {
    // Joseph drove here
    // otherwise, call file_write
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
  // Sahithi drove here
  if (fd < 0 || fd > 127)
  {
    return;
  }
  // look for file as long as it is not null
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
  // Sahithi drove here
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
  // Ashish drove here
  //checks for valid fd
  if (fd < 2 || fd > 127)
  {
    return;
  }
  // Pranay drove here
  struct thread *curr = thread_current();
  struct file *file = curr->fileDir[fd];
  if (file == NULL)
  {
    return;
  }
  // Ashish drove here
  // close file and make it NULL in file directory
  lock_acquire(&lock);
  file_close(file);
  curr->fileDir[fd] = NULL;
  lock_release(&lock);
}

// helper method that checks if ptr is valid
bool
check_valid(char* cmd_line)
{
  // Sahithi drove here
  struct thread *curr = thread_current();
  // checks for valid user address
  if (cmd_line == NULL || !is_user_vaddr(cmd_line))
  {
    return false;
  } 
  // Pranay drove here
  // checks for valid page directory
  else if (pagedir_get_page (curr->pagedir, cmd_line) == NULL)
  {
    return false;
  }
  // otherwise is valid
  return true;
}
