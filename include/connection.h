#pragma  once


#include <string>

enum class State{
  READING_REQUEST, 
  CONNECTING_BACKEND,
  FORWARDING_REQUEST,
  READING_RESPONSE,
  FORWARDING_RESPONSE,
  CLOSED
};

struct Connection {
  int client_fd;
  int backend_fd = -1;

  State state = State::READING_REQUEST;

  std::string request_buffer;
  std::string response_buffer;

  size_t response_sent = 0;

  bool backend_closed = false;

  Connection(int fd): client_fd(fd){}
};

