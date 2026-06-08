#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <cstdint>
#include <fcntl.h>
#include "socket.h"
int create_server_socket(int port){
  int server_fd = socket(AF_INET , SOCK_STREAM , 0);

  if(server_fd < 0){
    perror("socket");
    return -1;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  int opt = 1;
  setsockopt(server_fd , SOL_SOCKET , SO_REUSEADDR , &opt , sizeof(opt));

  if(bind(server_fd , (sockaddr*)&server_addr , sizeof(server_addr)) < 0){
    perror("bind");
    return -1;
  }

  if(listen(server_fd , 5) <0){
    perror("listen");
    return 1;
  }
  
  //make server non blocking
  make_non_blocking(server_fd);
  
  return server_fd;
}


void make_non_blocking(int fd ){
  int flags = fcntl(fd , F_GETFL , 0);

  if(flags == -1){
    perror("fcntl F_GETFL");
  
    exit(1);
  }
  if(fcntl(fd , F_SETFL , flags | O_NONBLOCK) == -1){
    perror("fcntl F_SETFL");
    exit(1);
  }
}


int connect_to_backend(const std::string &host , uint16_t port){
  int fd = socket(AF_INET , SOCK_STREAM , 0);

  if(fd < 0){
    perror("socket");
    return -1;
  }
  
  make_non_blocking(fd);


  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  
  if(inet_pton(AF_INET , host.c_str() , &addr.sin_addr) <= 0){
    perror("inet_pton");
    close(fd);
    return -1;
  }

  int ret = connect(fd , (sockaddr*)&addr , sizeof(addr));


  if(ret < 0 && errno != EINPROGRESS){
    perror("connect");
    close(fd);
    return -1;
  }
  return fd;
}
