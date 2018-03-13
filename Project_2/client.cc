#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "ClientRDT.h"
#include "Constants.h"
#include "Packet.h"

int main(int argc, char *argv[])
{
    int sockfd;  // socket descriptor
    int portno;
    string filename;
    struct sockaddr_in serv_addr;
    // contains tons of information, including the server's IP address.
    struct hostent *server;
    if (argc < 4) {
       fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
       exit(1);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create a new socket
    if (sockfd < 0) {
        util::exit_on_error("opening socket");
    }   

    server = gethostbyname(argv[1]);
    if (server == nullptr) {
        util::exit_on_error("no such host");
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    //establish a connection to the server.
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        fprintf(stderr, "ERROR connecting.\n");
    {
        ClientRDT client_conn(sockfd);
        if (client_conn.connected()) {
            filename = string(argv[3]);
        } else {
            util::exit_on_error("could not connect");
        }
        filename.resize(256);
        filename.replace(filename.end(), filename.begin() + 256,
                256 - filename.length(), '\0');
        filename[255] = '\0';
        cout << distance(filename.begin(), filename.end()) << endl;
        client_conn.Write(filename, 256);
        string s;
        client_conn.Read(s, 1);
        ofstream ofs("./received.data");
        client_conn.Read(s, 20);
        stringstream ss(s);
        size_t size;
        ss >> size;
        cout << "Size: " << size << endl;
        client_conn.Read(s, size);
        ofs << s;
    }

    close(sockfd);  // close socket

    return 0;
}
