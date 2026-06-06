#include<iostream>
#include<sys/socket.h>
#include<cstdint>
#include "socket.h"

std::string forward_to_backend(int fd , std::string raw){
  int bytes_send = send(fd , raw.data(), raw.size() , 0 );
  if(bytes_send <= 0){
    perror("send backend");
    exit(1);
  }
  char buffer[1024];
  int bytes_read = recv(fd , buffer , sizeof(buffer) , 0);
  
  std::string response(buffer , bytes_read);

  return response;
  
}
std::string proxy(std::string req , uint16_t port){
// request forward and recieve 
 int backend_fd = connect_to_backend("127.0.0.1" , port);

 std::string response = forward_to_backend(backend_fd ,req);
  
 return response;
}
