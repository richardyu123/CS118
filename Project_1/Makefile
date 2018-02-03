.SILENT:
CXX=g++
CXXFLAGS= -O2 -g -Wall -Wextra -std=c++11
FILES=server.cc
OUT=server

server:
	$(CXX) $(CXXFLAGS) server.cc -o server

default: server

clean:
	rm -f $(OUT)
