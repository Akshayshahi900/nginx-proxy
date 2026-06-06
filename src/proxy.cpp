#include<iostream>
#include<sys/socket.h>


std::string forward_to_backend(int fd , std::string raw){
  int bytes_send = send(fd , raw , raw.size() , 0 );
  if(bytes_send <= 0){
    perror("send backend");
    return;
  }
  char buffer[1024];
  int bytes_read = recv(fd , buffer , sizeof(buffer) , 0);
  
  return buffer;
  
}
std::string proxy(std::string req , std::string port){
// request forward and recieve 
 int backend_fd = create_tcp_connection(port);

 std::string response = forward_to_backend(backend_fd ,raw);
  
 return response;
 

}
