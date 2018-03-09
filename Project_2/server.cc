#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

void error(string msg) {
    fprintf(stderr, "ERROR: %s.\n", msg.c_str());
    exit(1);
}

int main(int argc, char** argv) {
    int sock_fd, port_no;
    struct sockaddr_in serv_addr;

    if (argc < 2) {
        error("No port provided");
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) { error("opening socket"); }
    memset(&serv_addr, 0, sizeof(serv_addr));
    port_no = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port_no);

    if (bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error("binding");
    }

    while(true) {

    }

    close(sock_fd);
    return 0;
}
