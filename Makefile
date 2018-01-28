CXX=g++
CXXFLAGS= -O2 -g -Wall -Wextra -std=c++11
MAIN=main.cc

default:
	$(CXX) $(CXXFLAGS) $(MAIN) -o main

clean:
	rm main
