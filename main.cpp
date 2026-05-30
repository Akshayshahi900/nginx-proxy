#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<cstring>


int main(){

  // create socket
  int server_fd = socket(AF_INET , SOCK_STREAM , 0);

  if(server_fd < 0){
    perror("socket");
    return 1;
  }

  // define server address
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(8080);

  // bind
  if(bind(server_fd , (sockaddr*)&server_addr , sizeof(server_addr))< 0){
    perror("bind");
    return 1;
  }
  
  // listen
  if(listen(server_fd, 5)<0){
    perror("listen");
    return 1;
  }

  std::cout << "Server listening on port 8080\n";

  //accept
  int client_fd = accept(server_fd, nullptr , nullptr);

  if(client_fd <0){
    perror("accept");
    return 1;
  }

  std::cout << "Client connected\n";

  char buffer[1024];

  // read data from client

  ssize_t bytes_read = read(client_fd , buffer , sizeof(buffer));

  if(bytes_read >0){

    //echo
    write(client_fd , buffer , bytes_read);

  }

  close(client_fd);
  close(server_fd);
  return 0;
}
