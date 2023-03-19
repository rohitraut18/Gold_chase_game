#ifndef fancyRW_h
#define fancyRW_h
#include<string.h> 
#include<stdio.h> 
#include<stdlib.h> 
#include<errno.h>
#include <sys/types.h>
#include<sys/socket.h>
#include<unistd.h> 
#include<netdb.h>

template<typename T>
int READ(int fd, T* obj_ptr, int counter)
{
  int a = counter, danny = 0;
  char* addr=(char*)obj_ptr;
  
  while(a > 0){
    danny = read(fd, addr, a);

    if (danny == -1 && errno == EINTR)
    { 
      danny = 0;
      continue;
    }
    else if(danny == -1)
    { 
      return -1;
    }
    else if(danny == 0)
    { 
      break;
    }
  	a -= danny;
  	addr += danny;
  }
  return counter; 
}

template<typename T>
int WRITE(int fd, T* obj_ptr, int counter)
{
  int a = counter, dabu = 0;
  char* addr=(char*)obj_ptr;
  
  while(a > 0)
  {
    dabu = write(fd, addr, a);

    if (dabu == -1 && errno == EINTR)
    { 
      dabu = 0;
      continue;
    }
    else if(dabu == -1)
    { 
      return -1;
    }
    a -= dabu;
    addr += dabu;
  }
  return counter; 
}

#endif
