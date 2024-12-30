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
#include <map>

enum arg_type {
    COMMAND,
    MSG,
    RECEIVE
};
enum MessageType {
    Login,
    Register,
    Authentication,
    List,
    ResourceRequest
};
enum State {
    Active = 1,
    Offline = 0
};

struct User
{
    std::string name;
    std::string ip;
    State state;
    User()
    { } 
    User(std::string name, std::string ip, State s)
    {
        this->name = name;
        this->ip = ip;
        this->state = s;
    }
    User(const User& other): 
        name(other.name),
        ip(other.ip),
        state(other.state)
    { }
};

namespace Request {
    struct ResourceRequest {
        std::string resource;
        std::string username;
        ResourceRequest():
            resource(""),
            username("")
        {}
        ResourceRequest(
            std::string resource,
            std::string username
        ):
            resource(resource),
            username(username)
        {}
    };
}

namespace Resolve {
    struct ResourceRequest {
        std::string resource;
        std::string ipAddress;
        ResourceRequest():
            resource(""),
            ipAddress("")
        {}
        ResourceRequest(
            std::string resource,
            std::string ipAddress
        ):
            resource(resource),
            ipAddress(ipAddress)
        {}
    };
}
#define THREAD_WAIT 1
#define ResourceTable std::map<std::string, User>

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

bool isResourceIpMsg(std::stack<std::string>& s);
bool isAuthenticationMsg(std::stack<std::string>& s);
MessageType identify(std::stack<std::string>& s);
std::map<std::string, User> parseResources(std::string s);
std::string getUsername(std::string line);
State getState(std::string line);
std::string parseResourceName(std::string line);
std::string parseResourceUser(std::string line);
void displayResourceTable(ResourceTable table);
Request::ResourceRequest getUserChoice(ResourceTable table);
int setup_server_address_dynamic(struct sockaddr_in& serv_addr, arg_type msg_type, std::string address);
void send_get_request(int sock, std::string filename, std::string folder);
int accept_connection(int server_fd, struct sockaddr_in& address);
void handle_client(int new_socket, std::string msg);
void close_connection(int socket_fd);
int start_listening(int server_fd);
#endif