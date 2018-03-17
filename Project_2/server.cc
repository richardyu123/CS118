#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
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

#include "Parameters.h"
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

        // Receive socket for filename.
        serv_conn.Receive(filename, 256);
        ifstream inFile(filename, ios::binary|ios::in);
        string file_data;
        if (inFile.fail()) {
            // Send that file wasn't found.
            cout << "ifs failed." << endl;
            serv_conn.Send(string("0"));
        } else {
            stringstream ss;

            inFile >> noskipws;

            inFile.seekg(0, ios::end);
            auto end = inFile.tellg();
            file_data.reserve(end);
            inFile.seekg(0, ios::beg);
            auto begin = inFile.tellg();

            //file_data.assign(istreambuf_iterator<char>(inFile),
            //                 istreambuf_iterator<char>());
            copy_n(istream_iterator<char>(inFile), (end - begin),
                   back_inserter(file_data));

            // Confirm that file was found, send filesize.
            serv_conn.Send(string("1"));
            ss << setfill('0') << setw(16) << (end - begin);
            serv_conn.Send(ss.str());

            // Send file data.
            serv_conn.Send(file_data);
        }
    }

    fprintf(stderr, "closing socket\n");
    close(sock_fd);
    return 0;
}
