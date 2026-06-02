#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<cstring>
#include "parser.h"
#include "request.h"

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
  
  // free socket to reuse it under TIME_WAIT
  int opt =1;
  setsockopt(
      server_fd, SOL_SOCKET , SO_REUSEADDR , &opt , sizeof(opt)
      );


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
  while(true){
  //accept
  int client_fd = accept(server_fd, nullptr , nullptr);

  if(client_fd <0){
    perror("accept");
    return 1;
  }

  std::cout << "Client connected\n";

  char buffer[1024];

  // read data from client

  ssize_t bytes_read = recv(client_fd , buffer , sizeof(buffer), 0);

  if(bytes_read < 0){
    perror("read");
    return 1;
  }

  // convert bytes_read to raw and parseRequest using HTTP parser
  
//  printf("%s", bytes_read);

  std::string raw(buffer , bytes_read);

  //call function of http parseRequest

  HttpRequest req = parseRequest(raw);

  

  std::cout << "Method: " << req.method << '\n';

  std::cout << "Path: " << req.path << '\n';

  std::cout << "Version: " << req.version << '\n';

  for (auto & [k , v] :req.headers){
    std::cout << k << " => " << v << '\n';
  }
  
  std::cout<<"Body: \n";

  for(auto s:req.body){
    std::cout << s ;
  }
  std::cout << "\n\n\nThe request data ends\n\n\n";
 
  //TODO Make a better response and calculate length of response body without hardcoding
  std::string response = "HTTP/1.1 200 OK\r\n"
                  "Content-Length: 2\r\n"
                  "\r\n"
                  "OK\n";

  int bytes_send = send(client_fd , response.c_str() , response.size() , 0);
  
  if(bytes_send == -1){
    perror("send");
  }


      
  close(client_fd);
  } 
  close(server_fd);
  return 0;
}
