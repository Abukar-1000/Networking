#ifndef SERVER_FUNCTIONS_EXISTS
#define SERVER_FUNCTIONS_EXISTS

#include <iostream>
#include <arpa/inet.h> // For inet_ntoa and inet_pton
#include <netinet/in.h> // For sockaddr_in structure
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>   
#include <sstream>
#include <unordered_map>
#include <queue>
#include "../ports.h"

namespace std {
    namespace filesystem = __fs::filesystem;
}

enum arg_type {
    COMMAND,
    MSG
};

// Final 
std::string parsePath(std::string path, std::string folder);
std::vector<std::string> getResources(std::string folder);

// Project 3
int create_socket(arg_type msg_type);
int bind_socket(int server_fd, struct sockaddr_in& address);
int start_listening(int server_fd);
int accept_connection(int server_fd, struct sockaddr_in& address);
void handle_client(int new_socket, std::string msg);
int setup_server_address(struct sockaddr_in& serv_addr, arg_type msg_type);
void close_connection(int socket_fd);
int handle_command_request(void);
arg_type find_arg_type(const char* arg);

int test_msgs();
void add_client(struct sockaddr_in& client, std::unordered_map<std::string*, struct sockaddr_in>* users);
void broadcast_message(std::string* msg, std::queue<std::string*>* messages);
void start_broadcast(int chat_room_fd, std::queue<std::string*>* messages, std::unordered_map<std::string*, struct sockaddr_in>* users);

#endif