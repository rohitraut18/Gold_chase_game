#include<iostream>
#include "goldchase.h"
#include <fcntl.h>  
#include <fstream>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include "Map.h"
#include <cstring>
#include <signal.h>
#include <mqueue.h>       
#include <sys/stat.h>        
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include<cstring>
#include <cstdio>
#include <errno.h>
#include <cstdlib>
#include <sstream>
#include <memory>
#include "fancyRW.h"
#define PORT "55735"  
#define PORT1 "55456"  
using namespace std;

#define  SHM_SM_NAME "/PD_semaphore"
#define  SHM_NAME "/PD_SharedMemory"


struct mapBoard
{
  unsigned short rows;
  unsigned short cols;
  pid_t pid[5];
  pid_t daemonID;
  unsigned char map[0];
};

void s_msg(char PLR_MASK, string msg);
void clearing(int);
vector< char >  ipc_conn(unsigned short & rows, unsigned short & cols, string ip_address);
void mq_de(int current_player);
void c_connect();
void d2plr(int toPlayerInt, string msg);
void sp_prot(char protocol_type);
void invokin_in_daemon( void (*f) (string), string);
void sm_prot(char protocol_type);
void serv_dem(string);
void s_map(char protocol_type);
void h_commun();
void ic_daemon(string);
void m_que(int current_player);


string mqueue_name[5] =  {"/rr0","/rr1","/rr2","/rr3","/rr4"}; 
unsigned char player_n[5] = {G_PLR0,G_PLR1,G_PLR2,G_PLR3,G_PLR4};
shared_ptr<Map> pointer_to_rendering_map; 
mqd_t readqueue_fd; 
sem_t* mysemaphore;
mapBoard * gmp = NULL;
int current_player = 0, thisPlayerLoc= 0;
char initial_map[2100];
int write_fd = -1;
int read_fd = -1;
mqd_t daemon_readqueue_fds[5]; 
int position=0;
int player_number;
bool win_msg_pass = false;


vector< char >  ipc_conn( unsigned short & rows, unsigned short & cols, string ip_address)
{
  int sockfd, status; //file descriptor for the socket
  const char* portno= PORT;
  char * ip_cstr = new char[ip_address.length()+1];
  strcpy(ip_cstr, ip_address.c_str());

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

  struct addrinfo *servinfo;
  if((status=getaddrinfo(ip_cstr, portno, &hints, &servinfo))==-1)
  {fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));exit(1);}

  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {perror("connect");exit(1);}

  freeaddrinfo(servinfo);

  char initial_map[2100];

  READ<unsigned short>(sockfd, &rows, sizeof(unsigned short));
  READ<unsigned short>(sockfd, &cols, sizeof(unsigned short));

  vector< char >  mbpVector(rows*cols , '*');

  READ<char>(sockfd, initial_map, (rows*cols + 1)*sizeof(char));

  for (int i=0; i < rows*cols; i++)
      mbpVector[i] = initial_map[i];
  read_fd = sockfd;
  delete [] ip_cstr;
  return mbpVector;
}

int get_Write_Socket_fd(string ip_address)
{
  int sockfd; 
  int status; 
  const char* portno = PORT1;
  char * ip_cstr = new char[ip_address.length()+1];
  strcpy(ip_cstr, ip_address.c_str());

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); 
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

  struct addrinfo *servinfo;
  if((status=getaddrinfo(ip_cstr, portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("connect");
    exit(1);
  }
  freeaddrinfo(servinfo);
  delete [] ip_cstr;
  return sockfd;
}


void c_connect()
{
  int sockfd, status, iter = 0; 
  unsigned short rows,cols;
  const char* portno = PORT;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints)); 
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype=SOCK_STREAM; 
  hints.ai_flags=AI_PASSIVE; 

  struct addrinfo *servinfo;
  if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
  {fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));exit(1);}

  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  int yes=1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
  {perror("setsockopt");exit(1);}
  if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {perror("bind");exit(1);}
  freeaddrinfo(servinfo);
  if(listen(sockfd,1)==-1)
  {perror("listen");exit(1);}
  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);
  int new_sockfd;
  if((new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize))==-1)
  {perror("accept");exit(1);}
  rows = gmp->rows;
  cols = gmp->cols;
  for (int i=0; i < rows*cols; i++)
      initial_map[i] =  gmp->map[i];
  WRITE<unsigned short>(new_sockfd, &rows, sizeof(unsigned short));
  WRITE<unsigned short>(new_sockfd, &cols, sizeof(unsigned short));
  WRITE<char>(new_sockfd, initial_map, (rows*cols + 1)*sizeof(char));
  write_fd = new_sockfd;
}


void sp_prot(char protocol_type)
{
  sem_wait(mysemaphore);
  for(int i = 0; i < 5;i++ )
  {
      if(i==0 &&  (protocol_type & G_PLR0) && gmp->pid[i] == -1)
      { 
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if(i==0 &&  !(protocol_type & G_PLR0) && gmp->pid[i] != -1)
      { 
        gmp->pid[i] = -1;

        mq_close(daemon_readqueue_fds[i]);
        mq_unlink(mqueue_name[i].c_str());
        daemon_readqueue_fds[i] = -1;
      }
      if ( i==1 && (protocol_type & G_PLR1) && gmp->pid[i] == -1)
      { 
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if(i==1 &&  !(protocol_type & G_PLR1) && gmp->pid[i] != -1)
      { 
        gmp->pid[i] = -1;

        mq_close(daemon_readqueue_fds[i]);
        mq_unlink(mqueue_name[i].c_str());
        daemon_readqueue_fds[i] = -1;
      }

      if ( i==2 && (protocol_type & G_PLR2) && gmp->pid[i] == -1)
      { 
        gmp->pid[i] = getpid();
        mq_de(i);

      }
      if(i==2 &&  !(protocol_type & G_PLR2) && gmp->pid[i] != -1)
      { 
        gmp->pid[i] = -1;

        mq_close(daemon_readqueue_fds[i]);
        mq_unlink(mqueue_name[i].c_str());
        daemon_readqueue_fds[i] = -1;
      }

      if ( i==3 && (protocol_type & G_PLR3) && gmp->pid[i] == -1)
      { 
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if(i==3 &&  !(protocol_type & G_PLR3) && gmp->pid[i] != -1)
      { 
        gmp->pid[i] = -1;

        mq_close(daemon_readqueue_fds[i]);
        mq_unlink(mqueue_name[i].c_str());
        daemon_readqueue_fds[i] = -1;
      }

      if ( i==4 && (protocol_type & G_PLR4) && gmp->pid[i] == -1)
      {
        gmp->pid[i] = getpid();
        mq_de(i);

      }
      if(i==4 &&  !(protocol_type & G_PLR4) && gmp->pid[i] != -1)
      { 
        gmp->pid[i] = -1;
        mq_close(daemon_readqueue_fds[i]);
        mq_unlink(mqueue_name[i].c_str());
        daemon_readqueue_fds[i] = -1;
      }
    }
  sem_post(mysemaphore);

  if(protocol_type == -128 )
  {
    char protocol_type1 = 1;
    WRITE <char>(write_fd, &protocol_type1, sizeof(char));
  }
}


unsigned int act_plr()
{
  unsigned int mask = 0;
  if(gmp->pid[0] != -1 ){
    mask |= G_PLR0;
  }
  if(gmp->pid[1] !=  -1 ){
    mask |= G_PLR1;
  }
  if(gmp->pid[2] !=  -1 ){
    mask |= G_PLR2;
  }
  if(gmp->pid[3] !=  -1 ){
    mask |= G_PLR3;
  }
  if(gmp->pid[4] !=  -1 ){
    mask |= G_PLR4;
  }
return mask;
}

int get_Read_Socket_fd()
{
  int sockfd; 
  int status; 

 const char* portno = PORT1;
 struct addrinfo hints;
 memset(&hints, 0, sizeof(hints)); //zero out everything in structure
 hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
 hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
 hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me

 struct addrinfo *servinfo;
 if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
 {
   fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
   exit(1);
 }
 sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
 int yes=1;
 if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
 {
   perror("setsockopt");
   exit(1);
 }
 if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
 {
   perror("bind");
   exit(1);
 }
 
 freeaddrinfo(servinfo);

 if(listen(sockfd,1)==-1)
 {
   perror("listen");
   exit(1);
 }

 struct sockaddr_in client_addr;
 socklen_t clientSize=sizeof(client_addr);
 int new_sockfd;
 if((new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize))==-1)
 {
   perror("accept");
   exit(1);
 }
 return new_sockfd;
}



void socket_Player_signal_handler(int)
{
  char plr_mask = act_plr();
  char protocol_type = G_SOCKPLR;
  protocol_type |= plr_mask;
  WRITE <char>(write_fd, &protocol_type, sizeof(char));
}

void socket_Map_signal_handler(int)
{
  unsigned short rows, cols;
  vector<pair<short,char> > mapChangesVector;
  rows = gmp->rows;
  cols = gmp->cols;
  for (int i=0; i < rows*cols; i++)
  {
      if(initial_map[i] !=  gmp->map[i])
      {
        mapChangesVector.push_back(make_pair(i, gmp->map[i] ));
        initial_map[i] =  gmp->map[i];
      }
  }
  if(mapChangesVector.size() > 0)
  {
    char protocol_type = 0, changedMapValue;
    short changedMapId;
    int Vector_size = mapChangesVector.size();
    WRITE <char>(write_fd, &protocol_type, sizeof(char));
    WRITE <int>(write_fd, &Vector_size, sizeof(int));
    for(int i = 0; i<Vector_size; i++)
    {
      changedMapId = mapChangesVector[i].first;
      changedMapValue = mapChangesVector[i].second;
      WRITE <short>(write_fd, &changedMapId, sizeof(short));
      WRITE <char>(write_fd, &changedMapValue, sizeof(char));
    }
  }
}

void s_msg(char PLR_MASK, string msg)
{
  int msg_length = msg.length() + 1;
  char protocol_type = G_SOCKMSG;
  char *cstr = new char[msg_length];
  strcpy(cstr, msg.c_str());
  protocol_type |= PLR_MASK;
  printf("in client : msglen %d - msg - %s\n",msg_length, cstr);
  WRITE <char>(write_fd, &protocol_type, sizeof(char));
  WRITE <int>(write_fd, &msg_length, sizeof(int));
  WRITE <char>(write_fd, cstr, msg_length*sizeof(char));
  delete [] cstr;
}

string receiveMessagebyDaemon(mqd_t read_fd)
{
  int err;
  char msg[251];
  memset(msg, 0, 251); 
  string msg_str  = "";
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(read_fd, &mq_notification_event);

  while((err=mq_receive(read_fd, msg, 250, NULL))!=-1)
  {
    msg_str = msg;
    memset(msg, 0, 251);
  }
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
  }
  return msg_str;

}


void socket_Message_signal_handler(int)
{
  char p_mask = G_SOCKMSG;
  string msg_str;

  for(int i = 0; i<5;i++){
    if (daemon_readqueue_fds[i] != -1){
        msg_str = receiveMessagebyDaemon(daemon_readqueue_fds[i]);

        if(msg_str != "" && i == 0)
        {
          s_msg(p_mask | G_PLR0, msg_str);
        }
        if(msg_str != "" && i == 1)
        {
          s_msg(p_mask | G_PLR1, msg_str);
        }
        if(msg_str != "" && i == 2)
        {
          s_msg(p_mask | G_PLR2, msg_str);
        }
        if(msg_str != "" && i == 3)
        {
          s_msg(p_mask | G_PLR3, msg_str);
        }
        if(msg_str != "" && i == 4)
        {
          s_msg(p_mask | G_PLR4, msg_str);
        }
    }
  } 

}

void sm_prot(char active_plr_mask)
{
  int msg_length = 0, toPlayerInt;
  char msg_cstring[100];

  READ <int>(read_fd, &msg_length, sizeof(int));
  READ <char>(read_fd, msg_cstring, msg_length*sizeof(char));

  string msg(msg_cstring);

  for(int i = 0; i < 5;i++ ){
    if(i==0 &&  (active_plr_mask & G_PLR0) ){
      d2plr(i, msg);
    }
    if(i==1 &&  (active_plr_mask & G_PLR1) ){
      d2plr(i, msg);
    }
    if(i==2 &&  (active_plr_mask & G_PLR2) ){
      d2plr(i, msg);
    }
    if(i==3 &&  (active_plr_mask & G_PLR3) ){
      d2plr(i, msg);
    }
    if(i==4 &&  (active_plr_mask & G_PLR4) ){
      d2plr(i, msg);
    }
  }

}

void s_map(char protocol_type)
{
  int Vector_size;
  char changedMapValue;
  short changedMapId;

  vector<pair<short,char> > mapChangesVector;

  READ <int>(read_fd, &Vector_size, sizeof(int));

  for (int i=0; i<Vector_size; i++){

    READ <short>(read_fd, &changedMapId, sizeof(short));
    READ <char>(read_fd, &changedMapValue, sizeof(char));

    mapChangesVector.push_back(make_pair(changedMapId,changedMapValue));
  }

  sem_wait(mysemaphore);
  for(int i = 0; i<Vector_size; i++){
    gmp->map[mapChangesVector[i].first] = mapChangesVector[i].second;
  }
  sem_post(mysemaphore);
  
  for(int i=0; i<5; i++)
  {
    if(gmp->pid[i] != -1 && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID )
    {
      kill(gmp->pid[i], SIGUSR1);
    }
  }

}


void h_commun()
{
  char protocol_type = ' ' ; int return_code;
  return_code = READ <char>(read_fd, &protocol_type, sizeof(char));
  if(return_code == -1){
  }

  if (protocol_type&G_SOCKPLR ){
    sp_prot(protocol_type);

  }
  else if (protocol_type&G_SOCKMSG ){
    sm_prot(protocol_type);

  }
  else if (protocol_type == 0 ){
    s_map(protocol_type);

  }
  else if (protocol_type == 1 ){
      char protocol_type1 = 2;
      WRITE <char>(write_fd, &protocol_type1, sizeof(char));
      close(write_fd);
      close(read_fd);
      exit(0);
  }
  else if (protocol_type == 2 )
  {
    shm_unlink("/goldmemory");
    sem_close(mysemaphore);
    sem_unlink("/multiplayer_sema");

    close(write_fd);
    close(read_fd);
    exit(0);
  }
}
void set_d_s()
{
  struct sigaction exit_action;
  exit_action.sa_handler = socket_Player_signal_handler;
  exit_action.sa_flags=0;
  sigemptyset(&exit_action.sa_mask);
  sigaction(SIGHUP, &exit_action, NULL);

  struct sigaction my_sig_handler;
  my_sig_handler.sa_handler = socket_Map_signal_handler;
  sigemptyset(&my_sig_handler.sa_mask);
  my_sig_handler.sa_flags=0;
  sigaction(SIGUSR1, &my_sig_handler, NULL);

  struct sigaction action_to_take;
  action_to_take.sa_handler=socket_Message_signal_handler;
  sigemptyset(&action_to_take.sa_mask);
  action_to_take.sa_flags=0;
  sigaction(SIGUSR2, &action_to_take, NULL);
}

void serv_dem(string ip_address)
{
  unsigned short rows, cols;
  set_d_s();
  sem_wait(mysemaphore);
  int f1, size;
  f1 = shm_open("/goldmemory",O_RDWR, S_IRUSR|S_IWUSR);
  read(f1,&rows,sizeof(unsigned short));
  read(f1,&cols,sizeof(unsigned short));
  size = (rows*cols + sizeof(mapBoard));
  ftruncate(f1, size);
  gmp = (mapBoard*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, f1, 0);

  rows = gmp->rows;
  cols = gmp->cols;

  for (int i=0; i < rows*cols; i++)
      initial_map[i] =  gmp->map[i];
  initial_map[rows*cols] = '\0';

  daemon_readqueue_fds[0] = -1;daemon_readqueue_fds[1] = -1;daemon_readqueue_fds[2] = -1;daemon_readqueue_fds[3] = -1;daemon_readqueue_fds[4] = -1;
  gmp->daemonID = getpid();
  sem_post(mysemaphore);

  c_connect();
  char active_plr_mask = act_plr();
  WRITE <char>(write_fd, &active_plr_mask, sizeof(char));
  read_fd = get_Read_Socket_fd();
  while(1)
  {
    h_commun(); 
  }
}


void ini_actplr(char active_plr_mask)
{
    for(int i = 0; i < 5;i++ ){
      if(i==0 &&  (active_plr_mask & G_PLR0) && gmp->pid[i] == -1){
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if ( i==1 && (active_plr_mask & G_PLR1) && gmp->pid[i] == -1){
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if ( i==2 && (active_plr_mask & G_PLR2) && gmp->pid[i] == -1){
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if ( i==3 && (active_plr_mask & G_PLR3) && gmp->pid[i] == -1){
        gmp->pid[i] = getpid();
        mq_de(i);
      }
      if ( i==4 && (active_plr_mask & G_PLR4) && gmp->pid[i] == -1){
        gmp->pid[i] = getpid();
        mq_de(i);
      }
    }
}

void ic_daemon(string ip_address)
{
  unsigned short rows, cols;
  int goldCount, fd;
  daemon_readqueue_fds[0] = -1;daemon_readqueue_fds[1] = -1;daemon_readqueue_fds[2] = -1;daemon_readqueue_fds[3] = -1;daemon_readqueue_fds[4] = -1;


  vector< char >  mbpVector = ipc_conn(rows, cols, ip_address);

  char active_plr_mask;
  READ <char>(read_fd, &active_plr_mask, sizeof(char));

  mysemaphore=sem_open("/multiplayer_sema",O_CREAT,S_IRUSR|S_IWUSR,0);
  fd = shm_open("/goldmemory",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
  int size;
  size = (rows*cols + sizeof(mapBoard));
  ftruncate(fd, size);
  gmp = (mapBoard*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  gmp->rows = rows;
  gmp->cols = cols;
  gmp->pid[0] = -1; gmp->pid[1] = -1;gmp->pid[2] = -1;gmp->pid[3] = -1;gmp->pid[4] = -1;
  ini_actplr(active_plr_mask);
  for (int i=0; i < rows*cols; i++)
  {
    gmp->map[i] = mbpVector[i];
  }
  write_fd = get_Write_Socket_fd( ip_address);
  gmp->daemonID = getpid();
  sem_post(mysemaphore);
  set_d_s();

  while(1)
  {
    h_commun(); 
  }
}


void invokin_in_daemon( void (*f) (string)  ,string ip_address)
{
  if(fork() > 0)
    return;

  if(fork()>0)
    exit(0);

  if(setsid()==-1)
    exit(1);

  for(int i=0; i<sysconf(_SC_OPEN_MAX); ++i)
    close(i);

  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2
  umask(0);
  chdir("/");
  (*f)(ip_address);
}

void map_handler(int)
{
  if(pointer_to_rendering_map != NULL)
  {
    pointer_to_rendering_map->drawMap();
  }
}

void clearing(int)            
{
  sem_wait(mysemaphore);
  gmp->map[thisPlayerLoc] &= ~current_player;
  gmp->pid[player_number-1] = -1;
  sem_post(mysemaphore);
  for(int i=0; i<5; i++)
  {
    if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
    { 
      kill(gmp->pid[i], SIGUSR1);
    }
  }
  if(gmp->daemonID != -1 )
    {
      kill(gmp->daemonID, SIGHUP);
    }
  if(gmp->daemonID != -1 )
    {
      kill(gmp->daemonID, SIGUSR1);
    }
  mq_close(readqueue_fd);
  mq_unlink(mqueue_name[player_number-1].c_str());
  if(gmp->pid[0] == -1 && gmp->pid[1] == -1 && gmp->pid[2] == -1 && gmp->pid[3] == -1 && gmp->pid[4] == -1)
  {
     shm_unlink("/goldmemory");
     sem_close(mysemaphore);
     sem_unlink("/multiplayer_sema");
  }
  exit(0);

}

void m_que(int current_player)
{
    struct mq_attr mq_attributes;
    mq_attributes.mq_flags=0;
    mq_attributes.mq_maxmsg=10;
    mq_attributes.mq_msgsize=120;
    if((readqueue_fd=mq_open(mqueue_name[current_player].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,S_IRUSR|S_IWUSR, &mq_attributes))==-1)
    {
      cerr<<current_player<<"             ";
      perror("mq_open1");
      // clearing(0);
    }
    struct sigevent mq_notification_event;
    mq_notification_event.sigev_notify=SIGEV_SIGNAL;
    mq_notification_event.sigev_signo=SIGUSR2;
    mq_notify(readqueue_fd, &mq_notification_event);
}


void mq_de(int thisPlayerNumber)
{
  struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=120;

  if((daemon_readqueue_fds[thisPlayerNumber] = mq_open(mqueue_name[thisPlayerNumber].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
          S_IRUSR|S_IWUSR, &mq_attributes))==-1)
  {
    
    perror("mq_open2");
  }
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
}

void writing_in_queue(int player_num2)
{ 

int i;
unsigned int players, player_num;
string tmp, sst, message;
int m = 0;
for(int i = 0; i<5; i++)
  {
    if(gmp->pid[i]!=-1)
    {
      m|=player_n[i];
    }
  }
  m &= ~player_n[player_num2-1];
  player_num = pointer_to_rendering_map->getPlayer(m);
	if(player_num ==0)
		return;
	tmp = pointer_to_rendering_map->getMessage();
	sst = to_string(player_num2);
	message = "Player " + sst + " says: ";
	message.append(tmp);
	switch(player_num)
	{
		case G_PLR0: i = 0;
			     break;
		case G_PLR1: i = 1;
			     break;
		case G_PLR2: i = 2;
			     break;
		case G_PLR3: i = 3;
			     break;
		case G_PLR4: i = 4;
			     break;
	}
	mqd_t writequeue_fd;
	if((writequeue_fd =  mq_open(mqueue_name[i].c_str(), O_WRONLY|O_NONBLOCK)) == -1)
	{
		perror("mq open error at write mq");
		exit(1);
	}
	char message_text[121];
	memset(message_text , 0 ,121);
	strncpy(message_text, message.c_str(), 120);
	if(mq_send(writequeue_fd, message_text, strlen(message_text), 0) == -1)
	{
		perror("mq_send error");
		exit(1);
	}
	mq_close(writequeue_fd);
}

void d2plr(int toPlayerInt, string msg)
{
  mqd_t wq;
  if((wq=mq_open(mqueue_name[toPlayerInt].c_str(), O_WRONLY|O_NONBLOCK))==-1)
  {
    perror("Error in mq_send");
    // clearing(0);
  }
  char message_text[251];
  memset(message_text, 0, 251);
  const char *s = msg.c_str();
  strncpy(message_text, s, 250);
  if(  mq_send(wq, message_text, strlen(message_text), 0) == -1)
  {
      perror("Error in mq_send");
      // clearing(0);
  }
  mq_close(wq);
}


void broadcasting_queue(int player_num2)
{
int c = 0;
for(int i=0; i<5; i++)
  {
    if(gmp->pid[i]!=-1)
    {
      c++;
    }
  }
	string tmp,sst,message;
	if(c==1)
  {
    (*pointer_to_rendering_map).postNotice("NO PLAYERS PRESENT!!");
    return;
  }
  if(win_msg_pass == false)
	{
		tmp = pointer_to_rendering_map->getMessage();
		sst = to_string(player_num2);
		message = "Player " + sst + " says: ";
		message.append(tmp);
	}
	else if(win_msg_pass == true)
	{
		sst = to_string(player_num2);
		message = "Player " + sst + " has already won ";
	}
	mqd_t writequeue_fd;
	char message_text[240];
	for(int i = 0; i < 5; i++)
	{
		if(gmp->pid[i] != -1 && gmp->pid[i] != getpid())
		{
      cout<<mqueue_name[i]<<"        ";
			if((writequeue_fd=mq_open(mqueue_name[i].c_str(), O_WRONLY|O_NONBLOCK))==-1)
			{
				perror("mq_open2");
				exit(1);
			}

			memset(message_text, 0, 240);
			strncpy(message_text, message.c_str(), 240);
			if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
			{
				perror("mq_send3");
				exit(1);
			}
			mq_close(writequeue_fd);
		}
	}
	return;
}


void read_queue(int){ 
  int err;
  char msg[140];
  memset(msg, 0, 140);
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);


  while((err=mq_receive(readqueue_fd, msg, 140, NULL))!=-1)
  {
    if(pointer_to_rendering_map != NULL)
      (*pointer_to_rendering_map).postNotice(msg);
    memset(msg, 0, 140);
  }
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
  }

}

bool hasNonLocalPlayers()
{
  int d_pid = gmp->daemonID;
  if (gmp->pid[0] == d_pid || gmp->pid[1] == d_pid || gmp->pid[2] == d_pid || gmp->pid[3] == d_pid || gmp->pid[4] == d_pid)
    return true;
  else
  return false;

}
int main(int argc, char *argv[])
{
  int fgold=0,rgold=0;
  unsigned short rows=0, cols=0;
  int goldCount,fd,size;
  string ip_address = "";
  string s;
  bool inServerNode = false, inClientNode = false, mapu = false,if_win=false;
  if(argc == 2)
  {
  s = argv[1];
  if(isdigit(s[0]))
  {

    ip_address = argv[1];
    inClientNode = true;
    mysemaphore = sem_open("/multiplayer_sema" ,O_RDWR,S_IRUSR|S_IWUSR,1);
    if(mysemaphore == SEM_FAILED)
    {
     invokin_in_daemon(ic_daemon, ip_address);
      while(1)
      { 
        sleep(1);
        if ( (fd = shm_open("/goldmemory", O_RDONLY, S_IRUSR|S_IWUSR)) == -1)
        {
        
        }
        else
        {
          break;
        }
      }
    }
  }
  else
  {
    mapu = true;
  }
  }
  
  if(mapu == true || argc==1 )
  { 
      inServerNode = true;
  }

  mysemaphore = sem_open("/multiplayer_sema" ,O_RDWR,S_IRUSR|S_IWUSR,1);
  if(mysemaphore == SEM_FAILED && mapu==true)
  {
      string line;
      char c;
      ifstream mapStream;
      mapStream.open(argv[1]);                              //opening the file given by user
      if(!mapStream)
      {
        perror ("Couldn't open file 1!\n");  //checking if the file exist or not. If error opening the file, then following is done.
        sem_close(mysemaphore);
        sem_unlink("/multiplayer_sema");
        exit(1);                  
      }
      if(mapStream.is_open()) 
      {
        getline(mapStream, line);  //getting the text file and reading each line to get the number of rows and columns in total.
        goldCount = atoi(line.c_str());   
        while(getline(mapStream,line))    
        {
          ++rows;
          cols=line.length()>cols? line.length():cols;
        }
      }
      mapStream.close();  
      mysemaphore=sem_open("/multiplayer_sema",O_CREAT,S_IRUSR|S_IWUSR,1);
      sem_wait(mysemaphore);  
      int size;
      fd = shm_open("/goldmemory",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
      size = (rows*cols + sizeof(mapBoard)); 
      int ft = ftruncate(fd, size);
      if(ft==-1) //if ftruncate is unable to expand the memory, it will give this error.
      {
      perror("Error in ftruncate");
      sem_close(mysemaphore); //closing semaphore
      sem_unlink("/multiplayer_sema"); //removing semaphore
      close(fd); //closing the shared memory.
      shm_unlink("/goldmemory"); //removing the shared memory.
      exit(1); 
      }
      gmp = (mapBoard*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0); ///same first player mmap
      gmp->rows = rows;   
      gmp->cols = cols;
      for(int i = 0; i < 5; i++)
      {
      gmp->pid[i] = -1;
      }
      gmp->daemonID = -1; 
      unsigned char * mp;  
      mp = gmp->map; 
      mapStream.open(argv[1]);  //opening map
      if(!mapStream.is_open())
      {
      cerr << "Couldn't open file!\n";  //error opening file
      sem_close(mysemaphore);//close and remove semaphore
      sem_unlink("/multiplayer_sema");
      exit(1);                  
      }
      getline(mapStream, line);
      while(getline(mapStream,line))
      {
      for(int i=0; i<line.length(); ++i)   //reading map
      {
        if(line[i]=='*')
        {
          gmp->map[position]=G_WALL;
          position++;
        }
        else if(line[i]==' ')
        {
          gmp->map[position]=0;
          position++;
        }
        else
        {
          perror("Unknown characters in map file");
          close(fd);
          shm_unlink("/goldmemory");
          sem_close(mysemaphore);
          sem_unlink("/multiplayer_sema");
          exit(1);
        }
      }
    }
    mapStream.close(); 
  position = rand()%(rows*cols);
  if(goldCount!=0)
      {
      for(int i=0;i<goldCount-1;i++)      
        { 
        position=rand()%(rows*cols);
        
        while(gmp->map[position]!=0)
        {
          position=rand()%(rows*cols);
        }
        gmp->map[position] |= G_FOOL;
        }
       position=rand()%(rows*cols);
        while(gmp->map[position]!=0)  
        {
            position=rand()%(rows*cols);
        }
        gmp->map[position] |= G_GOLD;
      }
      
      gmp->pid[0]= getpid();
      player_number = 1;
      thisPlayerLoc=rand()%(rows*cols);
      while(gmp->map[thisPlayerLoc]!=0)   
      {
        thisPlayerLoc=rand()%(rows*cols);
      }
      current_player=G_PLR0;
      gmp->map[thisPlayerLoc] |= current_player; 
     sem_post(mysemaphore);
     if(inServerNode)
     {
      //invoking_daemon(in_server_d, ip_address);
      invokin_in_daemon(serv_dem, ip_address);
      while(1){ 
        if (gmp->daemonID != -1){break;}
      } 
     }
     pointer_to_rendering_map=make_shared<Map>(gmp->map, gmp->rows, gmp->cols);
     pointer_to_rendering_map->postNotice("WELCOME !!");  
   }
   else
   {
      sem_wait(mysemaphore);
        fd = shm_open("/goldmemory",O_RDWR, S_IRUSR|S_IWUSR);
        if(fd==-1)
        {
        perror("Error in shm_open");
        sem_close(mysemaphore);//close semaphore
        exit(1);
        } 
    unsigned short  rows; 
    unsigned short cols;
    int rows1 = read(fd,&rows,sizeof(unsigned short)); //read the rows from the shared memory's map to get the size of it.
    if(rows1==-1) //if error opening rows
    { 
      perror("Reading rows"); 
      exit(1); 
    }
    int cols1 = read(fd,&cols,sizeof(unsigned short)); //read the cols from the shared memory's map to get the size of it.
    if(cols1==-1) //if error opening columns
    { 
      perror("Reading columns"); 
      exit(1); 
    }
    gmp = (mapBoard*) mmap(NULL, sizeof(mapBoard)+rows*cols, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
    if(gmp==MAP_FAILED)   //error handling
      {
      close(fd);
      sem_close(mysemaphore);//close semaphore
      perror("MMAP FAILED ");
      exit(1);
      }

     if((gmp->pid[0]==-1))      //iterate through gmp->players looking for avialble spot
      {
        gmp->pid[0] = getpid();
        current_player = G_PLR0;
        player_number = 1;
      }
      else if((gmp->pid[1]==-1))
      {
        gmp->pid[1] = getpid();
        current_player = G_PLR1;
        player_number = 2;
      }
      else if((gmp->pid[2]==-1))
      {
        gmp->pid[2] = getpid();
        current_player = G_PLR2;
        player_number = 3;
      }
      else if((gmp->pid[3]==-1))
      {
        gmp->pid[3] = getpid();
        current_player = G_PLR3;
        player_number = 4;
      }
      else if((gmp->pid[4]==-1))
      {
        gmp->pid[4] = getpid();
        current_player = G_PLR4;
        player_number = 5;
      }
      else                                                                            
      {
        perror("5 players are playing..please try later!");
        close(fd);
        sem_close(mysemaphore);
        exit(1);
      }
      thisPlayerLoc=rand()%(rows*cols);   
      while(gmp->map[thisPlayerLoc]!=0)    //take random no. until find the blank space or 0
      {
        thisPlayerLoc=rand()%(rows*cols);
      }
      gmp->map[thisPlayerLoc] |= current_player;   //assigne the current player to blank space

      sem_post(mysemaphore);

      if(inClientNode)
      {
        
        if(gmp->daemonID != -1 )
        {
        kill(gmp->daemonID, SIGHUP);
        }
        
        if(gmp->daemonID != -1 )
        {
        kill(gmp->daemonID, SIGUSR1);
        }
      }
     if(inServerNode && hasNonLocalPlayers())
      {
        if(gmp->daemonID != -1 )
        {
        kill(gmp->daemonID, SIGHUP);
        }
        
        if(gmp->daemonID != -1 )
        {
        kill(gmp->daemonID, SIGUSR1);
        }
      }
    if(inServerNode && !hasNonLocalPlayers())
      {
       
      }
    pointer_to_rendering_map=make_shared<Map>(gmp->map, gmp->rows, gmp->cols);
    pointer_to_rendering_map->postNotice("WELCOME TO GOLDCHASE");     //post to the map
  }
    m_que(player_number-1);
    struct sigaction exit_action;
    exit_action.sa_handler = clearing;
    exit_action.sa_flags=0;
    sigemptyset(&exit_action.sa_mask);
    sigaction(SIGINT, &exit_action, NULL);
    sigaction(SIGTERM, &exit_action, NULL);
    sigaction(SIGHUP, &exit_action, NULL);

    struct sigaction my_sig_handler;
    my_sig_handler.sa_handler = map_handler;
    sigemptyset(&my_sig_handler.sa_mask);
    my_sig_handler.sa_flags=0;
    sigaction(SIGUSR1, &my_sig_handler, NULL);

    struct sigaction action_to_take;
    action_to_take.sa_handler=read_queue;
    sigemptyset(&action_to_take.sa_mask);
    action_to_take.sa_flags=0;
    sigaction(SIGUSR2, &action_to_take, NULL);
    char keystroke;
    while((keystroke=pointer_to_rendering_map->getKey()) !='Q') //while loop to get the keystroke from the user. If the keystroke is Q, it will exit.
		{
			if(keystroke =='h') //if keystrok is 'h', the player will move left.
			{ 
        if((thisPlayerLoc%gmp->cols) != 0) //checking if the mod of the locations is 0 or not.
				{
          if(!((gmp->map[thisPlayerLoc-1]) & G_WALL)) //logic gate feature to get in this loop if the output is not 0.
          {
            sem_wait(mysemaphore);
						gmp->map[thisPlayerLoc] &= ~current_player; //turning OFF the current location bit of the player 
						thisPlayerLoc-=1; //decrementing the position.
						if((gmp->map[thisPlayerLoc]) == G_GOLD) //if the gold is found on that location
            {
              rgold = 1; //then increment the counter of real gold.
            }
            else if(((gmp->map[thisPlayerLoc]) & G_FOOL) == G_FOOL) //if the gold is fools gold.
						{
              fgold = 1; //incrementing the value of fools gold.
            } 				
						gmp->map[thisPlayerLoc] |= current_player; //turning ON the new location bit of the player.
            pointer_to_rendering_map->drawMap(); //drawing the map again by updating the position by 1 bit to the left.
            for(int i = 0; i<5; i++)	
            {	
              if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                kill(gmp->pid[i], SIGUSR1);
              }
            }
            if(gmp->daemonID != -1 )
            {
              kill(gmp->daemonID, SIGUSR1);
            }
            pointer_to_rendering_map->drawMap();
            sem_post(mysemaphore);
          }
				}
				else if(if_win) //if the player has already found the real gold, then the player will escape from the map where there is '0' bit;
				{
          sem_wait(mysemaphore);
          gmp->map[thisPlayerLoc] &= ~current_player;
					pointer_to_rendering_map->drawMap(); //redrawing the map after the player exits from the map
          for(int i = 0; i<5; i++)	
          {	
            if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                kill(gmp->pid[i], SIGUSR1);
              }
          }	
          if(gmp->daemonID != -1 )
          {
            kill(gmp->daemonID, SIGUSR1);
          }
          win_msg_pass = true;	
          broadcasting_queue(player_number);
          if(gmp->daemonID != -1 )
          {
            kill(gmp->daemonID, SIGUSR2);
          }
					pointer_to_rendering_map->postNotice("You Won the game and have escaped!"); //post message .
          pointer_to_rendering_map->drawMap();
          sem_post(mysemaphore);
					break; //getting out of the loop.
				}  
			}

    else if(keystroke =='j') //if the user presses 'j'.
			{ 
				if(((thisPlayerLoc+gmp->cols) < gmp->rows*gmp->cols))  //if the user is inside the boundary of the map column
				{  
          if(!((gmp->map[thisPlayerLoc+gmp->cols]) & G_WALL))
          {
            sem_wait(mysemaphore);
						gmp->map[thisPlayerLoc] &= ~current_player; //turning OFF the player bit.
						thisPlayerLoc+=gmp->cols; //incrementing the player position by 1 bit in column of the struct.
						if((gmp->map[thisPlayerLoc]) == G_GOLD)
            {
              rgold = 1;
            } 				
            else if(((gmp->map[thisPlayerLoc]) & G_FOOL) == G_FOOL)
						{
              fgold = 1;
            }
						gmp->map[thisPlayerLoc] |= current_player;
            pointer_to_rendering_map->drawMap();
            for(int i = 0; i<5; i++)	
            {	
              if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                    kill(gmp->pid[i], SIGUSR1);
              }
            }
            if(gmp->daemonID != -1 )
            {
              kill(gmp->daemonID, SIGUSR1);
            }
            pointer_to_rendering_map->drawMap();
            sem_post(mysemaphore);
				  }
        }
        else if(if_win)
				{
          sem_wait(mysemaphore);
          gmp->map[thisPlayerLoc] &= ~current_player;
					pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)	
          {	
            if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
            {
              kill(gmp->pid[i], SIGUSR1);
            }
          }	
          if(gmp->daemonID != -1 )
          {
           kill(gmp->daemonID, SIGUSR1);
          }
          win_msg_pass = true;	
          broadcasting_queue(player_number);
					pointer_to_rendering_map->postNotice("You Won the game and have escaped!");
					pointer_to_rendering_map->drawMap();
          if(gmp->daemonID != -1 )
            {
            kill(gmp->daemonID, SIGUSR2);
            }
          sem_post(mysemaphore);
					break;
				}
			}

    else if(keystroke =='k') //if the user presses 'k'.
			{ 
				if(((thisPlayerLoc-gmp->cols) > 0)) //if the user player is inside the map.
				{  
          if(!((gmp->map[thisPlayerLoc-gmp->cols]) & G_WALL))
          {
            sem_wait(mysemaphore);
						gmp->map[thisPlayerLoc] &= ~current_player;
						thisPlayerLoc-=gmp->cols; //updating the position to the upper bit of the current bit.
						if((gmp->map[thisPlayerLoc]) == G_GOLD)
            {
              rgold = 1;
            } 				
            else if(((gmp->map[thisPlayerLoc]) & G_FOOL) == G_FOOL)
						{
              fgold = 1;
            }
						gmp->map[thisPlayerLoc] |= current_player;
            pointer_to_rendering_map->drawMap();
            for(int i = 0; i<5; i++)	
            {	
              if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                kill(gmp->pid[i], SIGUSR1);
              }
            }
            if(gmp->daemonID != -1 )
                {
                kill(gmp->daemonID, SIGUSR1);
                }
            pointer_to_rendering_map->drawMap();
            sem_post(mysemaphore);
          }
        }
        else if(if_win)
				{
          sem_wait(mysemaphore);
          gmp->map[thisPlayerLoc] &= ~current_player;
					pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)	
          {	
            if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
            {
              kill(gmp->pid[i], SIGUSR1);
            }
          }	
          if(gmp->daemonID != -1 )
            {
            kill(gmp->daemonID, SIGUSR1);
            }
          win_msg_pass = true;	
          broadcasting_queue(player_number);
					pointer_to_rendering_map->postNotice("You Won the game and have escaped!");
					pointer_to_rendering_map->drawMap();
          if(gmp->daemonID != -1 )
            {
            kill(gmp->daemonID, SIGUSR2);
            }
          sem_post(mysemaphore);
					break;
				}
			}

      else if(keystroke =='l') //if the user presses 'l'
			{ 
				if((((thisPlayerLoc+1)%gmp->cols) != 0)) //if the mod of the user location and the total cols is not 0;
				{  
          if(!((gmp->map[thisPlayerLoc+1]) & G_WALL))
          {
            sem_wait(mysemaphore);
						gmp->map[thisPlayerLoc] &= ~current_player;
						thisPlayerLoc+=1;//incrementing the position to the right side of the current bit.
						gmp->map[thisPlayerLoc] |= current_player;
            if((gmp->map[thisPlayerLoc]) == G_GOLD)
            {
              rgold = 1;
            }
            else if(((gmp->map[thisPlayerLoc]) & G_FOOL) == G_FOOL)
						{
              fgold = 1;
            }				
						
            pointer_to_rendering_map->drawMap();
            for(int i = 0; i<5; i++)	
            {	
              if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                kill(gmp->pid[i], SIGUSR1);
              }
            }
            if(gmp->daemonID != -1 )
            {
              kill(gmp->daemonID, SIGUSR1);
            }
            pointer_to_rendering_map->drawMap();
            sem_post(mysemaphore);
          }
        }
        else if(if_win && (((thisPlayerLoc+1)%gmp->cols)==0))
				{
          sem_wait(mysemaphore);
          gmp->map[thisPlayerLoc] &= ~current_player;
					pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)	
          {	
            if(gmp->pid[i] != -1 && i != (player_number-1)&& gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
            {
              kill(gmp->pid[i], SIGUSR1);
            }
          }	
          if(gmp->daemonID != -1 )
          {
            kill(gmp->daemonID, SIGUSR1);
          }
          win_msg_pass = true;	
          broadcasting_queue(player_number);
					pointer_to_rendering_map->postNotice("You Won the game and have escaped!");
					gmp->map[thisPlayerLoc] &= ~current_player;
          pointer_to_rendering_map->drawMap();
          if(gmp->daemonID != -1 )
            {
            kill(gmp->daemonID, SIGUSR2);
            }
          sem_post(mysemaphore);
					break;
				}
			}
      else if(keystroke == 'm') //m
      {
        writing_in_queue(player_number); 
        if(gmp->daemonID != -1 )
        {
          kill(gmp->daemonID, SIGUSR2);
        }
      }
      else if(keystroke == 'b') //b
      {
        broadcasting_queue(player_number);
        if(gmp->daemonID != -1 )
        {
          kill(gmp->daemonID, SIGUSR2);
        }
      }
      if(rgold == 1)//this condition will check for the real gold. If the gold is real, then it will display following message.
			{
        sem_wait(mysemaphore);
				gmp->map[thisPlayerLoc] &= ~G_GOLD;
				rgold = 0;
				if_win = 1;
        for(int i=0; i<5; i++)
           {
            if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                kill(gmp->pid[i], SIGUSR1);
              }
            }
            if(gmp->daemonID != -1 )
            {
            kill(gmp->daemonID, SIGUSR1);
            }
				pointer_to_rendering_map->postNotice("You have Won!! You can make a way out");
        pointer_to_rendering_map->drawMap();
        sem_post(mysemaphore);
      }
      else if(fgold == 1)//if the found gold is fools gold, it will display following message.
			{
        sem_wait(mysemaphore);
				gmp->map[thisPlayerLoc] &= ~G_FOOL;
        for(int i=0; i<5; i++)
           {
              if(gmp->pid[i] != -1 && i != (player_number-1) && gmp->daemonID != -1 && gmp->pid[i] != gmp->daemonID)
              {
                kill(gmp->pid[i], SIGUSR1);
              }
            }
           if(gmp->daemonID != -1 )
            {
            kill(gmp->daemonID, SIGUSR1);
            }
				pointer_to_rendering_map->postNotice("It is a Fool's Gold.");
				fgold = 0;
        pointer_to_rendering_map->drawMap();
        sem_post(mysemaphore);
      }
    }

  if(gmp->daemonID != -1 )
      {
       kill(gmp->daemonID, SIGHUP);
      }
   clearing(0);
   return 0;
}

