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

namespace std {
    namespace filesystem = __fs::filesystem;
}

#define PORT 8080
#define CHAT_ROOM_PORT 8081
#define CHAT_ROOM_BRODCAST_PORT 8082

enum arg_type {
    COMMAND,
    MSG
};

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

int main() {
    /*
        Create chat app & use a thread to handle commands in the 
        background
    */
    std::thread command_handler_thread(handle_command_request);
    command_handler_thread.detach();

    test_msgs();
}


int setup_server_address(struct sockaddr_in& serv_addr, arg_type msg_type) {
    serv_addr.sin_family = AF_INET;

    if (msg_type == COMMAND) {
        serv_addr.sin_port = htons(PORT);
    }
    else if (msg_type == MSG) {
        serv_addr.sin_port = htons(CHAT_ROOM_PORT);
    }

    if (inet_pton(AF_INET, "10.183.70.66", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        return -1;
    }
    return 0;
}

int test_msgs() {
    std::cout << "Creating chat room. \n";
    int chat_room_fd = create_socket(MSG);
    if (chat_room_fd < 0) return -1;

    // recieve address
    struct sockaddr_in client_address;
    socklen_t client_address_size = sizeof(client_address);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CHAT_ROOM_PORT);

    if (bind_socket(chat_room_fd, address) < 0) return -1;

    std::cout << "Chat room is waiting for messages...\n" << std::endl;

    std::queue<std::string*>* messages = new std::queue<std::string*>();
    std::unordered_map<std::string*, struct sockaddr_in>* users = new std::unordered_map<std::string*, struct sockaddr_in>();
    // read msg
    size_t bytesRead = 0;
    while (true)
    {
        char msg_buffer[1024] = {0};
        bytesRead = recvfrom(
            chat_room_fd, 
            msg_buffer, 
            sizeof(msg_buffer), 
            0, 
            (struct sockaddr*) &client_address, 
            &client_address_size
        );

        std::stringstream msg_stream(msg_buffer);
        std::cout << "ss msg: " << msg_stream.str() << "\n";
        std::string* msg = new std::string(msg_stream.str());
        //check if the message is command
        arg_type msg_type = find_arg_type(msg->c_str());
        std::cout << "msg type: " << msg_type << "\n";
        // handle message here
        if (msg_type == MSG) {
            add_client(client_address, users);
            if (msg->size() > 0){
                messages->push(msg);
                start_broadcast(chat_room_fd, messages, users);
                msg_stream.clear();
            }
        }
        else{
            std::cout << "command hit \n";
            std::cout << "Chat Room FD: " << chat_room_fd << "\n";
            std::cout << "Message: " << msg_stream.str() << "\n";
            //make TCP socket so that we can communicate for commands via TCP
            int tcp_sock = create_socket(COMMAND);
            if (tcp_sock < 0) return -1;
            if (setup_server_address(client_address, COMMAND) < 0) return -1;
            if (connect(tcp_sock, (struct sockaddr*)&client_address, sizeof(client_address)) < 0) {
                std::cerr << "Connection failed" << std::endl;
                return -1;
            }
            handle_client(tcp_sock, msg_stream.str());
        }
        bytesRead = 0;
    }
}


void add_client(struct sockaddr_in& client, std::unordered_map<std::string*, struct sockaddr_in>* users) 
{
    std::string* user_address = new std::string(inet_ntoa(client.sin_addr));

    bool is_first_msg_from_user = true;
    for (auto it = users->begin(); it != users->end(); ++it) {
        is_first_msg_from_user = *(it->first) != *user_address;
    }
    std::cout << "\nrequest from: " << *user_address << " is first msg: " << is_first_msg_from_user << "\n";
    if (is_first_msg_from_user) {
        std::cout << "\n insert hit \n";
        users->insert(
            std::pair<std::string*, struct sockaddr_in>(user_address, client)
        );
    }
}

void broadcast_message(std::string* msg, std::queue<std::string*>* messages) 
{
    std::cout << "pushing: " << *msg << "\n";
    messages->push(msg);
}

void start_broadcast(int chat_room_fd, std::queue<std::string*>* messages, std::unordered_map<std::string*, struct sockaddr_in>* users)
{
    // brodcast address
    int chat_room_brodcast_fd = create_socket(MSG);
    if (chat_room_brodcast_fd < 0) {
        std::cout << "\nfailed to create brodcast bind\n";
    }

    bool msg_available = messages->size() > 0;
    if (msg_available)
    {
        while (messages->size())
        {
            std::string msg = *messages->front();
            std::cout << "broadcasting: " << msg << " size: " << users->size() << "room: " << chat_room_fd << "\n";
            for (
                auto user_itterator = users->begin();
                user_itterator != users->end();
                ++user_itterator
            )
            {
                struct sockaddr* client_address_data = (struct sockaddr*) &user_itterator->second;
                std::cout << "\n\n[ address: " << user_itterator->first << " | physical address: " << client_address_data << " ]" << "\n";
                
                // extract into function
                struct sockaddr_in client_address;
                client_address.sin_family = AF_INET;
                client_address.sin_port = htons(CHAT_ROOM_BRODCAST_PORT);
                
                if (inet_pton(AF_INET, user_itterator->first->c_str(), &(client_address.sin_addr)) < 0) {
                    std::cout << "Couldnt send data \n"
                    << " message: " << msg << "\n"
                    << " ip: " << user_itterator->first << "\n";
                }

                sendto(
                    chat_room_brodcast_fd, 
                    msg.c_str(), 
                    msg.size(), 
                    0, 
                    (struct sockaddr*) &client_address, 
                    sizeof(client_address)
                );
            }
            delete messages->front();
            messages->pop();
        }
    }
}


int handle_command_request(void) {
    int server_fd = create_socket(COMMAND);
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

        handle_client(new_socket, "");
    }

    close_connection(server_fd);
    return 0;
}
/**
 * @param msg_type The type of the file descriptor of the socket to use for the connection.
 * @return The file descriptor of the created socket.
 *
 * Return a TCP socket when sending commands.
 * Return a UDP socket when sending messages.
 * @throws None
 */
int create_socket(arg_type msg_type) {
    int sock;
    if (msg_type == COMMAND) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    }
    else if (msg_type == MSG) {
        // create UDP socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (sock < 0) {
        std::cerr << "Socket creation error" << std::endl;
    }
    return sock;
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
    if (listen(server_fd, 5) < 0) {
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
void handle_client(int new_socket, std::string msg) {
    std::cout << "Handling client connection..." << std::endl;
    std::cout << "Socket fd: " << new_socket << std::endl;
    std::cout << "Message: " << msg << std::endl;
    std::cout << "Message size: " << msg.size() << std::endl;
    char buffer[1024] = {0};
    if (msg.size() == 0){
        read(new_socket, buffer, 1024);
    }
    
    if (msg.size() > 0) {
        std::cout << "Command: " << msg << std::endl;
        //add msg to the buffer
        strcat(buffer, msg.c_str());
    }
    std::string command(buffer);
    std::cout << "Message: " << command << std::endl;

    std::string action = command.substr(0, command.find(' '));
    //converting action to uppercase
    std::transform(action.begin(), action.end(), action.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    std::string filename = command.substr(command.find(' ') + 1);

    std::string file_path = "files/" + filename;

    if (action == "%PUT") {
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
    } else if (action == "%GET") {
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
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
    }
    close_connection(new_socket);
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

/**
 * Determines the command line argument type between COMMAND and MSG
 * @param arg The array of command line argument
 * 
 * @return COMMAND if '%' is found in the first character of arg and MSG otherwise
 * 
 * @throws None
 */
arg_type find_arg_type(const char* arg) {
    const char command_char = '%';

    if (arg[0] == command_char)
        return COMMAND;
    else
        return MSG;
}