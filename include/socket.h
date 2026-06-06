#pragma once
#include<iostream>
#include <cstdint>

int create_server_socket(int port);
void make_non_blocking(int fd);
int connect_to_backend(const std::string &host , uint16_t port);
