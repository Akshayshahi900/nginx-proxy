#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<cstring>
#include<vector>
#include<cstdint>
#include<unordered_map>
#include<errno.h>

#include "connection.h"
#include "parser.h"
#include "request.h"
#include "socket.h"
#include "load_balancer.h"

#define MAX_EVENTS 64
#define BUFFER_SIZE 4096

std::unordered_map<int, Connection> connections;
int epoll_fd;
int server_fd;

bool has_full_request(const std::string& buf) {
    return buf.find("\r\n\r\n") != std::string::npos;
}

void handle_reading_request(Connection& conn) {
    char buf[BUFFER_SIZE];
    
    ssize_t n = recv(conn.client_fd, buf, BUFFER_SIZE, MSG_DONTWAIT);
    
    if (n == 0) {
        std::cout << "[" << conn.client_fd << "] Client closed\n";
        conn.state = State::CLOSED;
        return;
    }
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        perror("recv");
        conn.state = State::CLOSED;
        return;
    }
    
    conn.request_buffer.append(buf, n);
    std::cout << "[" << conn.client_fd << "] Received " << n << " bytes. Total: " 
              << conn.request_buffer.size() << "\n";
    
    if (has_full_request(conn.request_buffer)) {
        std::cout << "[" << conn.client_fd << "] Got full HTTP request\n";
        
        HttpRequest req = parseRequest(conn.request_buffer);
        std::cout << "[" << conn.client_fd << "] Method: " << req.method 
                  << ", Path: " << req.path << "\n";
        
        uint16_t backend_port = load_balancer(req);
        std::cout << "[" << conn.client_fd << "] Load balancer chose port: " << backend_port << "\n";
        
        conn.backend_fd = connect_to_backend("127.0.0.1", backend_port);
        
        if (conn.backend_fd < 0) {
            std::cout << "[" << conn.client_fd << "] Failed to create backend socket\n";
            conn.state = State::CLOSED;
            return;
        }
        
        std::cout << "[" << conn.client_fd << "] Connecting to backend...\n";
        conn.state = State::CONNECTING_BACKEND;
        
        epoll_event ev = {};
        ev.events = EPOLLOUT | EPOLLERR;
        ev.data.fd = conn.backend_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn.backend_fd, &ev);
    }
}

void handle_connecting_backend(Connection& conn) {
    int err = 0;
    socklen_t len = sizeof(err);
    
    int ret = getsockopt(conn.backend_fd, SOL_SOCKET, SO_ERROR, &err, &len);
    
    if (ret < 0 || err != 0) {
        std::cout << "[" << conn.client_fd << "] Backend connect failed: " 
                  << strerror(err) << "\n";
        conn.state = State::CLOSED;
        return;
    }
    
    std::cout << "[" << conn.client_fd << "] Backend connected successfully!\n";
    
    conn.state = State::FORWARDING_REQUEST;
    
    epoll_event ev = {};
    ev.events = EPOLLOUT | EPOLLIN;
    ev.data.fd = conn.backend_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn.backend_fd, &ev);
}

void handle_forwarding_request(Connection& conn) {
    ssize_t sent = send(conn.backend_fd, 
                       conn.request_buffer.c_str(),
                       conn.request_buffer.size(),
                       MSG_DONTWAIT);
    
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("send");
            conn.state = State::CLOSED;
        }
        return;
    }
    
    std::cout << "[" << conn.client_fd << "] Sent " << sent << "/" 
              << conn.request_buffer.size() << " bytes to backend\n";
    
    conn.request_buffer.erase(0, sent);
    
    if (conn.request_buffer.empty()) {
        std::cout << "[" << conn.client_fd << "] Full request sent, waiting for response\n";
        
        conn.state = State::READING_RESPONSE;
        
        epoll_event ev = {};
        ev.events = EPOLLIN;
        ev.data.fd = conn.backend_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn.backend_fd, &ev);
    }
}

void handle_reading_response(Connection& conn) {
    char buf[BUFFER_SIZE];
    
    ssize_t n = recv(conn.backend_fd, buf, BUFFER_SIZE, MSG_DONTWAIT);
    
    if (n == 0) {
        std::cout << "[" << conn.client_fd << "] Backend closed\n";
        conn.backend_closed = true;
        
        if (conn.response_buffer.empty()) {
            conn.state = State::CLOSED;
        } else {
            conn.state = State::FORWARDING_RESPONSE;

            epoll_event ev = {};
            ev.events = EPOLLOUT;
            ev.data.fd = conn.client_fd;
            epoll_ctl(epoll_fd , EPOLL_CTL_MOD , conn.client_fd , &ev);
        }
        return;
    }
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No more data right now
          if(!conn.response_buffer.empty()){
            conn.state = State::FORWARDING_RESPONSE;

            epoll_event ev ={};
            ev.events = EPOLLOUT;
            ev.data.fd = conn.client_fd;
            epoll_ctl(epoll_fd , EPOLL_CTL_MOD , conn.client_fd , &ev);
          }
          return;
        }
        perror("recv");
        conn.state = State::CLOSED;
        return;
    }
    
    conn.response_buffer.append(buf, n);
    std::cout << "[" << conn.client_fd << "] Received " << n << " bytes from backend\n";
    
    conn.state = State::FORWARDING_RESPONSE;

    epoll_event ev = {};
    ev.events = EPOLLOUT;
    ev.data.fd = conn.client_fd;
    epoll_ctl(epoll_fd  , EPOLL_CTL_MOD , conn.client_fd , &ev);
    
}
void handle_forwarding_response(Connection& conn) {
    if (conn.response_buffer.empty()) {
        if (conn.backend_closed) {
            conn.state = State::CLOSED;
        }
        return;
    }
    //if already sent all data
    if(conn.response_sent >= conn.response_buffer.size()){
      if(conn.backend_closed){
        conn.state = State::CLOSED;
      }
      else{
        conn.state = State::READING_RESPONSE;

        epoll_event ev ={};
        ev.events = EPOLLIN;
        ev.data.fd = conn.backend_fd;
        epoll_ctl(epoll_fd , EPOLL_CTL_MOD , conn.backend_fd , &ev);
      }
      return;
    }

    ssize_t sent = send(conn.client_fd,
                       conn.response_buffer.c_str() + conn.response_sent,
                       conn.response_buffer.size() - conn.response_sent,
                       MSG_DONTWAIT);
    
    if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("send client");
            conn.state = State::CLOSED;
        }
        return;
    }
    
    conn.response_sent += sent;
    std::cout << "[" << conn.client_fd << "] Sent " << sent << " bytes to client\n";
    
    if (conn.response_sent >= conn.response_buffer.size() && conn.backend_closed) {
        std::cout << "[" << conn.client_fd << "] Request complete\n";
        conn.state = State::CLOSED;
    }
}
int main(){
    server_fd = create_server_socket(8080);
    
    if(server_fd == -1){
        perror("Create Socket");
        return 1;
    }

    std::cout << "Server listening on PORT 8080\n" ;

    epoll_fd = epoll_create1(0);
    
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1){
        perror("epoll_ctl add server");
        return -1;
    }

    epoll_event events[MAX_EVENTS];

    // Main event loop
    while(true){
        int ready_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if(ready_count == -1){
            perror("epoll_wait");
            continue;
        }
        
        for(int i = 0; i < ready_count; i++){
            int fd = events[i].data.fd;

            // New client connection
            if(fd == server_fd){
                while(true){
                    int client_fd = accept(server_fd, nullptr, nullptr);

                    if(client_fd == -1){
                        break;
                    }

                    // Create connection object
                    connections.emplace(client_fd, Connection(client_fd));
                    
                    make_non_blocking(client_fd);

                    epoll_event client_event{};
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client_fd;

                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

                    std::cout << "Client Connected: " << client_fd << '\n';
                }
            }
            // Existing connection
            else{
                // Find which connection this fd belongs to
                Connection* conn_ptr = nullptr;
                
                // Is it a client fd?
                if(connections.count(fd)){
                    conn_ptr = &connections.at(fd);
                }
                // Is it a backend fd?
                else{
                    for(auto& [cfd, conn] : connections){
                        if(conn.backend_fd == fd){
                            conn_ptr = &conn;
                            break;
                        }
                    }
                }
                
                if(!conn_ptr) continue;

                if(events[i].events & EPOLLIN){
                    // Socket is readable
                    if(fd == conn_ptr->client_fd){
                        if(conn_ptr->state == State::READING_REQUEST){
                            handle_reading_request(*conn_ptr);
                        }
                    } else if(fd == conn_ptr->backend_fd){
                        if(conn_ptr->state == State::READING_RESPONSE){
                            handle_reading_response(*conn_ptr);
                        }
                    }
                }
                
                if(events[i].events & EPOLLOUT){
                    // Socket is writable
                    if(fd == conn_ptr->backend_fd){
                        if(conn_ptr->state == State::CONNECTING_BACKEND){
                            handle_connecting_backend(*conn_ptr);
                        } else if(conn_ptr->state == State::FORWARDING_REQUEST){
                            handle_forwarding_request(*conn_ptr);
                        }
                    } else if(fd == conn_ptr->client_fd){
                        if(conn_ptr->state == State::FORWARDING_RESPONSE){
                            handle_forwarding_response(*conn_ptr);
                        }
                    }
                }

                // Cleanup if closed
                if(conn_ptr->state == State::CLOSED){
                    if(conn_ptr->client_fd != -1){
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn_ptr->client_fd, nullptr);
                        close(conn_ptr->client_fd);
                    }
                    if(conn_ptr->backend_fd != -1){
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn_ptr->backend_fd, nullptr);
                        close(conn_ptr->backend_fd);
                    }
                    connections.erase(conn_ptr->client_fd);
                    std::cout << "Connection cleaned up\n";
                }
            }
        }
    }    

    close(server_fd);
    close(epoll_fd);

    return 0;
}
