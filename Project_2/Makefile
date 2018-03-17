CXX = g++
FLAGS = -Wall -Wextra -std=c++14
DIR = ./project_2
BASE_CC = Packet.cc RDTController.cc Parameters.cc
TEST_CC = PacketTest.cc
CLIENT_CC = client.cc ClientRDT.cc
SERVER_CC = server.cc ServerRDT.cc


default: dir client server test

dir: $(DIR)

$(DIR):
	mkdir -p $(DIR)

client: $(CLIENT_CC) $(BASE_CC)
	$(CXX) $(FLAGS) $^ -o $@
	
server: $(SERVER_CC) $(BASE_CC)
	$(CXX) $(FLAGS) $^ -o $@
		
test: $(TEST_CC) $(BASE_CC)
	$(CXX) $(FLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f client server test
	rm -f p2/project2_304464688_004493871.tar
	rmdir p2

dist:
	mkdir p2
	tar -cf p2/project2_304464688_004493871.tar README Makefile *.cc *h report.pdf
