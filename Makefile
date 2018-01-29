.SILENT:
CXX=g++
CXXFLAGS= -O2 -g -Wall -Wextra -std=c++11
FILES=server.cc
OUT=server

default:
	$(CXX) $(CXXFLAGS) $(FILES) -o $(OUT)

clean:
	rm $(OUT)
