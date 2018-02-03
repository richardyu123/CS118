#include <algorithm>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

using namespace std;

static string format_date = "%a, %d %b %Y %T GMT";

// Structure that contains the HTTP Response fields.
struct HeaderInfo {
    string header_line;
    string date;
    string last_modified;
    string server;
    int content_length;
    string connection;
    string content_type;
    bool failure;

    // Default header info.
    HeaderInfo() {
        header_line = "HTTP/1.1 200 OK";
        last_modified = date = "%a, %d %b %Y %T GMT";
        server = "webserver/0.0.1";
        content_length = 0;
        connection = "close";
        content_type = "application/octet-stream";
        failure = false;
    }

    // If requested file does not exist.
    void SetFailureMessage() {
        header_line = "HTTP/1.1 404 Not Found";
        connection = "close";
        content_type = "text/html";
        failure = true;
    }

    // Builds the response based on the field information.
    string GetResponse() {
        char buffer[1024];
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

// Maps file extension to MIME.
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
    int num_bytes_read;
    char req_buffer[1024];
    bzero(req_buffer, 1024);

    // Read the request from the socket
    num_bytes_read = read(sock_fd, req_buffer, 1023);
    if (num_bytes_read < 0) {
        error("ERROR reading from socket");
    }

    string request;
    request.append(req_buffer, num_bytes_read);
    printf("%s", request.c_str());

    size_t begin_location = 0;
    size_t end_location = request.find(' ', begin_location);
    if (end_location == string::npos) { return; }

    // Parse the request to get the uri
    begin_location = end_location + 1;
    end_location = request.find(' ', begin_location);
    if (end_location == string::npos) { return; }
    string uri = request.substr(begin_location, end_location - begin_location);

    // Convert "%20" to spaces in the uri
    string uri_with_spaces = "";
    size_t uri_begin_location = 0;
    size_t uri_end_location = 0;
    while (true) {
        uri_end_location = uri.find("%20", uri_begin_location);
        if (uri_end_location == string::npos) {
            uri_with_spaces += uri.substr(uri_begin_location, uri_end_location);
            break;
        }
        uri_with_spaces += uri.substr(uri_begin_location, uri_end_location - uri_begin_location) + ' ';
        uri_begin_location = uri_end_location + 3;
    }
    uri = uri_with_spaces;

    HeaderInfo header_info;

    // File path is from current directory ('.').
    string file_path = '.' + uri;
    string file_content;
    struct stat buf;
    int status = stat(file_path.c_str(), &buf);
    ifstream stream(file_path);

    // If failure, set failure = true, else build HTTP response from
    // the content in the file.
    if ((S_ISDIR(buf.st_mode) && !status) || stream.fail()) {
        header_info.SetFailureMessage();
        file_content = "<html><h1>404 Not Found</h1></html>";
        header_info.content_length = file_content.length();
    } else {
        file_content.assign((istreambuf_iterator<char>(stream)),
                            (istreambuf_iterator<char>()));
        header_info.content_length = file_content.length();

        size_t ext_index = uri.find_last_of('.', string::npos);
        if (ext_index != string::npos) {
            string ext = uri.substr(ext_index);
            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            auto iter = ext_to_MIME.find(ext);
            if (iter != ext_to_MIME.end()) {
               header_info.content_type = iter->second;
            }
        }

        if (!status) {
            char time_buffer[1024];
            struct tm* time = gmtime(&buf.st_mtime);
            status = strftime(time_buffer, 1024, format_date.c_str(), time);
            if (status != 0) {
                header_info.last_modified = string(time_buffer);
            }
        }
    } 

    // Get current time and add that to HTTP response.
    char time_buffer[1024];
    time_t current_time = time(nullptr);
    struct tm* time = gmtime(&current_time);
    status = strftime(time_buffer, 1024, format_date.c_str(), time);
    if (status) {
        header_info.date = string(time_buffer);
    }  
    auto response = header_info.GetResponse();
    response += "\r\n" + file_content;

    // Send response through socket.
    status = write(sock_fd, response.c_str(), response.length());
    if (status < 0) { error("error writing to socket."); }
}

int main(int argc, char* argv[]) {
    int sock_fd, new_sock_fd, port_no;
    socklen_t cli_len;
    struct sockaddr_in serv_addr, cli_addr;
    if (argc < 2) {
        fprintf(stderr, "usage: ./server <portnum>\n");
        exit(1);
    }
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0); // create socket
    if (sock_fd < 0) {
        error("ERROR opening socket.");
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr)); // reset memory

    // fill in address info
    port_no = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_no);

    if (bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("Error on binding.");
    }

    listen(sock_fd, 5);

    int p_id;
    while (true) {
        new_sock_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &cli_len);

        if (new_sock_fd < 0) {
            error("ERROR on accept.");
        }

        // One process generates the response, while the other listens for
        // requests.
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
