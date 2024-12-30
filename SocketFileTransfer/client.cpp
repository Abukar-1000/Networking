#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>

#define PORT 8080

int create_socket();
int setup_server_address(struct sockaddr_in& serv_addr);
int connect_to_server(int sock, struct sockaddr_in& serv_addr);
void send_put_request(int sock, const char* filepath);
void send_get_request(int sock, const char* filename);
void close_socket(int sock);

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./client [put/get] [filepath/filename]" << std::endl;
        return -1;
    }

    const char* command = argv[1];
    const char* filepath_or_filename = argv[2];

    int sock = create_socket();
    if (sock < 0) return -1;

    struct sockaddr_in serv_addr;
    if (setup_server_address(serv_addr) < 0) return -1;

    if (connect_to_server(sock, serv_addr) < 0) return -1;

    if (strcmp(command, "put") == 0) {
        send_put_request(sock, filepath_or_filename);
    } else if (strcmp(command, "get") == 0) {
        send_get_request(sock, filepath_or_filename);
    } else {
        std::cerr << "Unknown command. Use 'put' or 'get'." << std::endl;
    }

    close_socket(sock);
    return 0;
}

/**
 *
 * @return The file descriptor of the created socket.
 *
 * @throws None
 */
int create_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error" << std::endl;
    }
    return sock;
}

/**
 * Sets up the server address for the given sockaddr_in structure.
 *
 *
 * @return 0 on success, -1 on failure.
 *
 * @throws None
 */
int setup_server_address(struct sockaddr_in& serv_addr) {
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "10.0.0.129", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        return -1;
    }
    return 0;
}

/**
 * Establishes a connection to the server using the given socket and server address.
 * @param sock The file descriptor of the socket to use for the connection.
 * @param serv_addr The server address to connect to.
 *
 * @return 0 on successful connection, -1 on failure.
 *
 * @throws None
 */
int connect_to_server(int sock, struct sockaddr_in& serv_addr) {
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        close_socket(sock);
        return -1;
    }
    return 0;
}

/**
 *
 * @param sock The file descriptor of the socket to use for the file transfer.
 * @param filepath The path to the file to be sent.
 *
 * @return None
 *
 * @throws None
 */
void send_put_request(int sock, const char* filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "File not found: " << filepath << std::endl;
        return;
    }

    std::string filename = std::string("PUT ") + filepath;

    write(sock, filename.c_str(), filename.size() + 1);
    file.seekg(0, std::ios::beg);
    char buffer[1024] = {0};
    file.read(buffer, sizeof(buffer));
    size_t bytesWritten = write(sock, buffer, file.gcount());
    while (bytesWritten > 0) {
        file.read(buffer, sizeof(buffer));
        bytesWritten = write(sock, buffer, file.gcount());
    }
    std::cout << "File sent: " << filepath << std::endl;
}

/**
 * Sends a GET request to the server to retrieve a file.
 *
 * @param sock The file descriptor of the socket to use for the file transfer.
 * @param filename The name of the file to be retrieved.
 *
 * @return None
 *
 * @throws None
 */
void send_get_request(int sock, const char* filename) {
    std::string request = std::string("GET ") + filename;
    send(sock, request.c_str(), request.size() + 1, 0);

    char buffer[1024] = {0};
    std::ofstream outFile(filename, std::ios::binary);

    int bytesRead;
    while ((bytesRead = read(sock, buffer, sizeof(buffer))) > 0) {
        outFile.write(buffer, bytesRead);
    }

    if (bytesRead < 0) {
        std::cerr << "Failed to receive file." << std::endl;
    } else {
        std::cout << "File received: " << filename << std::endl;
    }
}

/**
 * Closes an open socket connection.
 * @param socket_fd The file descriptor of the socket to be closed.
 *
 * @return None
 *
 * @throws None
 */
void close_socket(int sock) {
    close(sock);
}
