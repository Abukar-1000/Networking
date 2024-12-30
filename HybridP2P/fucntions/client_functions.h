#ifndef CLIENT_FUNCTIONS_EXISTS
#define CLIENT_FUNCTIONS_EXISTS
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <thread>
#include <queue>
#include <mutex>

enum arg_type {
    COMMAND,
    MSG,
    RECEIVE
};

int create_socket(arg_type msg_type);
int setup_server_address(struct sockaddr_in& serv_addr, arg_type msg_type);
int connect_to_server(int sock, struct sockaddr_in& serv_addr);
int bind_socket(int server_fd, struct sockaddr_in& address);
void send_put_request(int sock, const char* filepath);
void send_get_request(int sock, const char* filename);

int send_command(int sock, const char* command, const char* filename);
void listen_for_message(std::queue<std::string>& incomming_messages, std::mutex& lock);
void display_incomming_messages(std::queue<std::string>& incomming_messages, std::mutex& lock);
void close_socket(int sock);
arg_type find_arg_type(const char* arg);

#endif