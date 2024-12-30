#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>   

namespace std {
    namespace filesystem = __fs::filesystem;
}

#define PORT 8080

int create_socket();
int bind_socket(int server_fd, struct sockaddr_in& address);
int start_listening(int server_fd);
int accept_connection(int server_fd, struct sockaddr_in& address);
void handle_client(int new_socket);
void close_connection(int socket_fd);
std::string parseFileName(std::string filePath);

int main() {
    int server_fd = create_socket();
    if (server_fd < 0) return -1;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind_socket(server_fd, address) < 0) return -1;
    if (start_listening(server_fd) < 0) return -1;

    std::cout << "Server is waiting for a connection..." << std::endl;

    while (true) {
        int new_socket = accept_connection(server_fd, address);
        if (new_socket < 0) return -1;

        //create a new thread for each client and detach it
        // STD::Thread. cppreference.com. (n.d.). https://en.cppreference.com/w/cpp/thread/thread 
        std::thread client_thread(handle_client, new_socket);
        client_thread.detach();
    }

    close_connection(server_fd);
    return 0;
}

/**
 * Creates a socket using the AF_INET address family and SOCK_STREAM socket type.
 *
 * @return The file descriptor of the created socket, or -1 if the socket creation fails.
 *
 * @throws None
 */
int create_socket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }
    return server_fd;
}

/**
 * Binds a socket to a specific address and port.
 *
 * @param server_fd The file descriptor of the server socket.
 * @param address The reference to the sockaddr_in object containing the address and port.
 * @return 0 if the bind operation is successful, -1 otherwise.
 *
 * @throws None.
 */
int bind_socket(int server_fd, struct sockaddr_in& address) {
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return -1;
    }
    return 0;
}

/**
 * Starts listening for incoming connections on a given server file descriptor.
 *
 * @param server_fd The file descriptor of the server socket.
 *
 * @return 0 if the listen operation is successful, -1 otherwise.
 *
 * @throws None
 */
int start_listening(int server_fd) {
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return -1;
    }
    return 0;
}

/**
 *
 * @param server_fd The file descriptor of the server socket.
 * @param address The reference to the sockaddr_in object containing the address and port of the incoming connection.
 *
 * @return The file descriptor of the new socket representing the accepted connection, or -1 if the accept operation fails.
 *
 * @throws None
 */
int accept_connection(int server_fd, struct sockaddr_in& address) {
    int addrlen = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (new_socket < 0) {
        std::cerr << "Accept failed" << std::endl;
        close(server_fd);
        return -1;
    }
    return new_socket;
}

/**
 * Handles a client connection by reading a command and performing the corresponding action.
 * @param new_socket The file descriptor of the new socket representing the accepted connection.
 *
 * @return None
 *
 * @throws None
 */
void handle_client(int new_socket) {
    char buffer[1024] = {0};
    char fileNameBuffer[100] = {0};

    // clear the values inside each buffer before using. 
    // Std::Memset. cppreference.com. (n.d.-a). https://en.cppreference.com/w/cpp/string/byte/memset 
    memset(buffer, 0, 1024);
    memset(fileNameBuffer, 0, 100);
    read(new_socket, fileNameBuffer, 100);

    std::string command(fileNameBuffer);
    std::string action = command.substr(0, command.find(' '));
    std::string filename = command.substr(command.find(' ') + 1);
    filename = parseFileName(filename);

    std::string file_path = "files/" + filename;
    if (action == "PUT") {
        if (std::filesystem::exists(file_path)) {
            std::cerr << "File already exists: " << filename << std::endl;
            close_connection(new_socket); 
            return;
        }

        std::ofstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to create file: " << filename << std::endl;
            close_connection(new_socket); 
            return;
        }

        int bytesRead;
        while ((bytesRead = read(new_socket, buffer, sizeof(buffer))) > 0) {
            file.write(buffer, bytesRead);
        }

        std::cout << "File saved: " << filename << std::endl;
    } else if (action == "GET") {
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "File not found: " << filename << std::endl;
            close_connection(new_socket);
            return;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            close_connection(new_socket); 
            return;
        }

        int bytesRead;
        while ((bytesRead = file.read(buffer, sizeof(buffer)).gcount()) > 0) {
            send(new_socket, buffer, bytesRead, 0);
        }

        std::cout << "File sent: " << filename << std::endl;
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
    }

    close_connection(new_socket);
}

/**
 * Given a file path, returns the file name
 * @param filePath The file path.
 *
 * @return None
 *
 * @throws None
 */
std::string parseFileName(std::string filePath) {
    std::string fileName = "";
    int fileNameStartIndex = 0;
    for (int i = 0; i < filePath.length(); ++i) {
        if (filePath[i] == '/') {
            fileNameStartIndex = i;
        }
    }

    int length = (filePath.length() - fileNameStartIndex);
    bool pathWasProvided = fileNameStartIndex != 0;
    
    // we return the parsed path name to get the filename
    if (pathWasProvided) {
        return filePath.substr(fileNameStartIndex + 1, length);
    }

    // Path was just the file name so we just return it
    return filePath;
}

/**
 * Closes an open socket connection.
 * @param socket_fd The file descriptor of the socket to be closed.
 *
 * @return None
 *
 * @throws None
 */
void close_connection(int socket_fd) {
    close(socket_fd);
}
