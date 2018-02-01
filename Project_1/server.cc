#include <algorithm>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unordered_map>
#include <iostream>

using namespace std;

static string format_date = "%a, %d %b %Y %T GMT";

struct HeaderInfo {
    string header_line;
    string date;
    string last_modified;
    string server;
    int content_length;
    string connection;
    string content_type;
    bool failure;

    HeaderInfo() {
        header_line = "HTTP/1.1 200 OK";
        last_modified = date = "%a, %d %b %Y %T GMT";
        server = "webserver/0.0.1";
        content_length = 0;
        connection = "close";
        content_type = "application/octet-stream";
        failure = false;
    }
    void SetFailureMessage() {
        header_line = "HTTP/1.1 404 Not Found";
        connection = "close";
        content_type = "text/html";
        failure = true;
    }

    string GetResponse() {
        char buffer[512];
        if (!failure) {
            sprintf(buffer,
                    "%s\r\nConnection: %s\r\nServer: %s\r\nContent-Type: %s\r\nLast-Modified: %s\r\nDate: %s\r\nContent-Length: %d\r\n",
                    header_line.c_str(), connection.c_str(), server.c_str(),
                    content_type.c_str(), last_modified.c_str(), date.c_str(),
                    content_length);
        } else {
            sprintf(buffer,
                    "%s\r\nConnection: %s\r\nServer: %s\r\nContent-Type: %s\r\nDate: %s\r\nContent-Length: %d\r\n",
                    header_line.c_str(), connection.c_str(), server.c_str(),
                    content_type.c_str(), date.c_str(), content_length);
        }
        string output(buffer);
        return output;
    }
};

const unordered_map<string, string> ext_to_MIME = {
    {".gif", "image/gif"},
    {".html", "text/html"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"}
};

void error(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void GenerateResponse(int sock_fd) {
    int n;
    char req_buffer[512];
    bzero(req_buffer, 512);
    string request;

    n = read(sock_fd, req_buffer, 511);
    if (n < 0) {
        error("ERROR reading from socket.");
    }

    request.append(req_buffer, n);
    printf("Got request: %s", request.c_str());

    size_t start = 0;
    size_t end = request.find(' ', start);
    if (end == string::npos) { return; }
    auto method = request.substr(start, end - start);

    start = end + 1;
    end = request.find(' ', start);
    if (end == string::npos) { return; }
    auto uri = request.substr(start, end - start);

    start = end + 1;
    end = request.find("\r\n", start);
    if (end == string::npos) { return; }
    auto version = request.substr(start, end - start);
    
    start = end + 2;

    HeaderInfo header_info;

    string path = "." + uri;
    ifstream stream(path);
    string content;
    struct stat buf;
    int status = stat(path.c_str(), &buf);
    if (stream.fail() || (status == 0 && S_ISDIR(buf.st_mode))) {
        header_info.SetFailureMessage();
        content = "<html><h1>404 Page Not Found<\h1><\html>";
        header_info.content_length = content.length();
    } else {
        stringstream buffer;
        buffer << stream.rdbuf();
        content = buffer.str();
        header_info.content_length = content.length();

        auto i = uri.rfind('.');
        if (i != string::npos) {
            auto ext = uri.substr(i);
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            auto iter = ext_to_MIME.find(ext);
            if (iter != ext_to_MIME.end()) {
               header_info.content_type = iter->second;
            }
        }

        if (status == 0) {
            char time_buffer[512];
            struct tm* time = gmtime(&buf.st_mtime);
            status = strftime(time_buffer, 512, format_date.c_str(), time);
            if (status != 0) {
                header_info.last_modified = string(time_buffer);
            }
        }

        char time_buffer[512];
        time_t current_time = time(nullptr);
        struct tm* time = gmtime(&current_time);
        status = strftime(time_buffer, 512, format_date.c_str(), time);
        if (status != 0) {
            header_info.date = string(time_buffer);
        }
    }
    printf("%s\n", uri.c_str());
    auto response = header_info.GetResponse();
    response += "\r\n\r\n" + content;
    status = write(sock_fd, response.c_str(), response.length());
    if (status < 0) { error("ERROR writing to socket."); }
    printf("%s", response.c_str());
}

int main(int argc, char* argv[]) {
    HeaderInfo header_info;
    cout << header_info.GetResponse().c_str();
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
            GenerateResponse(new_sock_fd);
            close(new_sock_fd);
            exit(0);
        } else {
            close(new_sock_fd);
        }
    }

    return 0;
}
