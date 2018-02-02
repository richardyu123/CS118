.SILENT:
CXX=g++
CXXFLAGS= -O2 -g -Wall -Wextra -std=c++11
FILES=server.cc client.cc
OUT=server client

server:
	$(CXX) $(CXXFLAGS) server.cc -o server

client:
	$(CXX) $(CXXFLAGS) client.cc -o client

default: server

clean:
	rm -f $(OUT)
