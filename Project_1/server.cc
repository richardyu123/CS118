#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unordered_map>

using namespace std;

struct HeaderInfo {
    string header_line;
    string date;
    int content_length;
    string keep_alive;
    string connection;
    string content_type;
    bool failure;

    HeaderInfo() {
        header_line = "HTTP/1.1 200 OK";
        date = "%a, %d %b %Y %T GMT";
        content_length = 0;
        keep_alive = "timeout=%d,max=%d";
        connection = "Keep-Alive";
        content_type = "application/octet-stream";
        failure = false;
    }
    void SetFailureMessage() {
        header_line = "HTTP/1.1 404 Not Found";
        connection = "close";
        content_type = "text/html";
        failure = true;
    }
};

const unordered_map<string, string> ext_to_MIME = {
    {".gif", "image/gif"},
    {".html", "text/html"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"}
};

void GenerateResponse(int sock_fd) {
    int n;
    char buffer[512];
    bzero(buffer, 512);
    string request;

    n = read(sock_fd, buffer, 511);
    if (n < 0) {
        error("ERROR reading from socket.");
    }

    request.append(buffer, n);
    printf("Got request: %s", request.c_str());

    size_t start = 0;
    size_t end = request.find(' ', start);
    if (end == string::npos) { return; }

    HeaderInfo header_info;
    header_info.content_type = ext_to_MIME.find(".gif")->second;
}

void error(const char* msg) {
    fprintf(stderr, "%s", msg);
    exit(1);
}

int main(int argc, char* argv[]) {
    int sock_fd, new_sock_fd, port_no;
    socklen_t cli_len;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr, "ERROR: No port provided\n");
        exit(1);
    }
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        error("ERROR opening socket.");
    }
    memset((char*)&serv_addr, 0, sizeof(serv_addr));

    port_no = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_no);

    if (bind(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Error on binding.");
    }

    listen(sock_fd, 5);

   /*
    * Part A.
    *
    new_sock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &cli_len);

    if (new_sock_fd < 0) {
        error("ERROR on accept.");
    }

    int n;
    char buffer[256];
    memset(buffer, 0, 256);

    n = read(new_sock_fd, buffer, 255);
    if (n < 0) { error("ERROR reading from socket."); }
    printf("Here is the message: %s\n", buffer);

    n = write(new_sock_fd, "I got your message", 18);
    if (n < 0) { error("ERROR writing to socket"); }
    */

    // Part B.

    int p_id;
    while (true) {
        new_sock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &cli_len);

        if (new_sock_fd < 0) {
            error("ERROR on accept.");
        }

        p_id = fork();
        if (p_id < 0) {
            error("ERROR on fork.");
        }
        
        if (p_id == 0) {
            close(sock_fd);
            GenerateResponse(sock_fd);
            close(new_sock_fd);
            exit(0);
        } else {
            close(new_sock_fd);
        }
    }

    return 0;
}
