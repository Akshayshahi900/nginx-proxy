#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<cstring>
#include<vector>

#include "parser.h"
#include "request.h"
#include <unordered_map>

#define MAX_EVENTS 64
#define BUFFER_SIZE 1024

struct Connection {
  int fd ;

  std::string read_buffer;
  std::string write_buffer;
};

std::unordered_map<int , Connection> connections;

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


  // make the socket non-blocking
  make_non_blocking(server_fd);


 // EPOLL INSTANCE
  int epoll_fd = epoll_create1(0);
  
  epoll_event event{};

  event.events = EPOLLIN;
  event.data.fd = server_fd;


  if(epoll_ctl(epoll_fd ,EPOLL_CTL_ADD , server_fd , &event) == -1){
    perror("epoll_ctl add server");
    return -1;
  }

   epoll_event events[MAX_EVENTS];

   //event loop
  while(true){
  //accept
  
    int ready_count = epoll_wait(epoll_fd , events , MAX_EVENTS , -1);

    if(ready_count == -1){
      perror("epoll_wait");
      continue;
    }
    
    for(int i =0 ; i < ready_count ; i++){
      int fd = events[i].data.fd;

      //create new connected
      if(fd == server_fd){
        while(true){
          int client_fd = accept(server_fd  , nullptr , nullptr);

          if(client_fd ==-1 ){
            break;
          }

          connections.emplace( client_fd , Connection{client_fd , "" , ""});

          
          make_non_blocking(client_fd);

          epoll_event client_event{};
          client_event.events = EPOLLIN;
          client_event.data.fd = client_fd;

          epoll_ctl(epoll_fd , EPOLL_CTL_ADD , client_fd , &client_event);

          std::cout << "Client Connected: " << client_fd << '\n';
        }
      }
      else{
        char buffer[BUFFER_SIZE];
        Connection& conn = connections[fd];
        int bytes_read = recv(conn.fd , buffer , sizeof(buffer) , 0);
        
        if(bytes_read <=0 ){
          std::cout << "Client disconnected:" << conn.fd << '\n';
          epoll_ctl(epoll_fd , EPOLL_CTL_DEL , conn.fd , nullptr);
          close(conn.fd);
          connections.erase(conn.fd);
          continue;
        }
        

        std:: string raw(buffer , bytes_read);
        
        conn.read_buffer.append(buffer , bytes_read);

        
        HttpRequest req = parseRequest(conn.read_buffer);

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

      //TODO Make a better response and calculate length of response body without hardcoding
        conn.write_buffer = "HTTP/1.1 200 OK\r\n"
                  "Content-Length: 2\r\n"
                  "\r\n"
                  "OK\n";

        int bytes_send = send(conn.fd , conn.write_buffer.data() ,conn.write_buffer.size() , 0);
  
        if(bytes_send == -1){
          perror("send");
        }

        conn.read_buffer.clear();
        conn.write_buffer.clear();
      }
    }
  }    

  for(auto& [fd , conn]: connections){
    close(fd);
  }

  close(server_fd);
  close(epoll_fd);

  return 0;
}
