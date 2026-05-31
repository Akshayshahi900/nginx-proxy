CXX = g++

CXXFLAGS = -g -Wall -Wextra -Wpedantic

TARGET = server

SRC = src/main.cpp src/parser.cpp

all:
	$(CXX)	$(CXXFLAGS)	$(SRC)	-Iinclude	-o	$(TARGET)
