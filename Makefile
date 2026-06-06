CXX = g++

CXXFLAGS = -g -Wall -Wextra -Wpedantic

TARGET = server

SRC = src/load_balancer.cpp	src/main.cpp	src/parser.cpp	src/proxy.cpp	src/socket.cpp

all:
	$(CXX)	$(CXXFLAGS)	$(SRC)	-Iinclude	-o	$(TARGET)
