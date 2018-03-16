CXX = g++
FLAGS = -Wall -Wextra -std=c++11
DIR = ./project_2
BASE_CC = Packet.cc RDTConnection.cc Parameters.cc
TEST_CC = PacketTest.cc
CLIENT_CC = client.cc ClientRDT.cc
SERVER_CC = server.cc ServerRDT.cc
BASE_O = $(BASE_CC:%.cc=$(DIR)/%.o)
TEST_O = $(TEST_CC:%.cc=$(DIR)/%.o)
CLIENT_O = $(CLIENT_CC:%.cc=$(DIR)/%.o)
SERVER_O = $(SERVER_CC:%.cc=$(DIR)/%.o)
BASE_DEP = $(BASE_O:%.o=%.d)
TEST_DEP = $(TEST_O:$.o=$.d)
CLIENT_DEP = $(CLIENT_O:%.o=%.d)
SERVER_DEP = $(SERVER_O:%.o=%.d)

default: dir client server test

dir: $(DIR)

$(DIR):
	mkdir -p $(DIR)

client: $(CLIENT_O) $(BASE_O)
	$(CXX) $(FLAGS) $^ -o $@
	
server: $(SERVER_O) $(BASE_O)
	$(CXX) $(FLAGS) $^ -o $@
		
test: $(TEST_O) $(BASE_O)
	$(CXX) $(FLAGS) $^ -o $@

-include $(BASE_DEP) $(TEST_DEP) $(CLIENT_DEP) $(SERVER_DEP)

$(DIR)/%.o: %.cc
	$(CXX) $(FLAGS) -MMD -c $< -o $@

.PHONY: clean

clean:
	rm -f $(BASE_O) $(TEST_O) $(CLIENT_O) $(SERVER_O) $(BASE_DEP) $(TEST_DEP) $(CLIENT_DEP) $(SERVER_DEP) client server test
	rmdir $(DIR)

