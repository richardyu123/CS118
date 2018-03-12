#include <fstream>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <streambuf>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Constants.h"
#include "ServerRDT.h"

using namespace std;

int main(int argc, char** argv) {
    int sock_fd, port_no;
    struct sockaddr_in serv_addr;

    if (argc < 2) {
        util::exit_on_error("No port provided");
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) { util::exit_on_error("opening socket"); }
    memset(&serv_addr, 0, sizeof(serv_addr));
    port_no = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port_no);

    if (bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        util::exit_on_error("binding");
    }

    while(true) {
        ServerRDT serv_conn(sock_fd);
        if (!serv_conn.connected()) { continue; }
        string filename;
        ostringstream oss;
        serv_conn.Read(oss, 256);
        cout << filename << endl;
        filename = oss.str();
        cout << "Filename: " << filename << endl;
        ifstream inFile(filename);
        string file_data;
        if (inFile.fail()) {
            cout << "ifs failed." << endl;
        }

        inFile.seekg(0, std::ios::end);
        file_data.reserve(inFile.tellg());
        inFile.seekg(0, std::ios::beg);

        file_data.assign(istreambuf_iterator<char>(inFile),
                   istreambuf_iterator<char>());

        serv_conn.Write(file_data);
    }

    fprintf(stderr, "closing socket\n");
    close(sock_fd);
    return 0;
}
