
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

#define PORT 8080
#define CHAT_ROOM_PORT 8081
#define CHAT_ROOM_BRODCAST_PORT 8082

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

int main(int argc, char const *argv[]) {
    
    int tcp_sock = create_socket(COMMAND);
    if (tcp_sock < 0) return -1;
    struct sockaddr_in serv_addr;
    if (setup_server_address(serv_addr, COMMAND) < 0) return -1;
    if (connect_to_server(tcp_sock, serv_addr) < 0) return -1;

    if (find_arg_type(argv[1]) == COMMAND) {
        if (argc != 3) {
            std::cerr << "Usage: ./client [%put/%get] [filepath/filename]" << std::endl;
            return -1;
        }
        const char* command = argv[1];
        const char* filepath_or_filename = argv[2];
        send_command(tcp_sock, command, filepath_or_filename);
    }
    // handle message
    else if (find_arg_type(argv[1]) == MSG)
    {
        const char* command = argv[1];
        const char* msg = argv[2];


        // UDP work here
        int chat_room_fd = create_socket(MSG);
        if (chat_room_fd < 0) return -1;
        struct sockaddr_in chat_room_server_addr;
        if (setup_server_address(chat_room_server_addr, MSG) < 0) return -1;
        
        std::string msg_string = "";
        // put inside a function
        for (int i = 1; i < argc; ++i) {
            msg_string += argv[i];
            msg_string += " ";
        }

        std::queue<std::string> incomming_messages;
        std::mutex global_lock;
        // https://stackoverflow.com/questions/22332181/passing-lambdas-to-stdthread-and-calling-class-methods [used]
        std::thread incomming_messages_listener(listen_for_message, std::ref(incomming_messages), std::ref(global_lock));
        incomming_messages_listener.detach();

        sendto(chat_room_fd, msg_string.c_str(), msg_string.size(), 0, (struct sockaddr*)&chat_room_server_addr, sizeof(chat_room_server_addr));
        std::cout 
            << "Options: \n" 
            << "\tMessage: \n" 
            << "\t\t Send message by typing out a message to send.\n" 
            << "\tCommand: \n" 
            << "\t\t [%put] [filepath/filename]\n" 
            << "\t\t [%get] [filepath/filename]\n";

        
        std::cout << "Chat room is " << chat_room_fd << std::endl;
        std::string user_input = "";
        std::string delimeter = " ";
        while (true)
        {
            display_incomming_messages(incomming_messages, global_lock);
            std::cout << ": ";
            std::getline(std::cin, user_input);
            display_incomming_messages(incomming_messages, global_lock);

            bool is_command_request = strcmp(command, "%") == 0;
            if (is_command_request) 
            {
                std::stringstream stream(user_input);
                std::string command, filepath_or_filename;
                stream >> command >> filepath_or_filename;
                send_command(tcp_sock, command.c_str(), filepath_or_filename.c_str());
            }
            else 
            {
                sendto(
                    chat_room_fd, 
                    user_input.c_str(), 
                    user_input.size(), 
                    0, 
                    (struct sockaddr*)&chat_room_server_addr, 
                    sizeof(chat_room_server_addr)
                );
                user_input = "";
            }
        }
    }

    
    return 0;
}

/**
 * @param incomming_messages Messages boradcasted from the server.
 * @param lock Global access lock to control adding and removing incoming messages.
 * @return void
 *  
 * Displays recieved messages to the client.
 * @throws None
 */
void display_incomming_messages(std::queue<std::string>& incomming_messages, std::mutex& lock) {

    lock.lock();

    while (incomming_messages.size()) {
        std::cout << "[new message]: " << incomming_messages.front() << " size: " << incomming_messages.size() << "\n";
        incomming_messages.pop();
    }
    
    lock.unlock();

}

/**
 * @param chat_room_fd File descriptor of the socket to use for the connection.
 * @param incomming_messages Messages boradcasted from the server.
 * @param lock Global access lock to control adding and removing incomming messages.
 * @return void
 *  
 * Listens for messages brodcasted from the server.
 * Used to notifies client of a new message.
 * @throws None
 */
void listen_for_message(std::queue<std::string>& incomming_messages, std::mutex& lock) {
    int chat_room_fd = create_socket(MSG);
    if (chat_room_fd < 0) {
        lock.lock();
        incomming_messages.push("Listener Failed to create socket.");
        lock.unlock();
    }

    // recieve address
    struct sockaddr_in client_address;
    socklen_t client_address_size = sizeof(client_address);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CHAT_ROOM_BRODCAST_PORT);

    if (bind_socket(chat_room_fd, address) < 0) {
        lock.lock();
        incomming_messages.push("Listener Failed to bind.");
        lock.unlock();
    }

    while (true) {
        char msg_buffer[1024] = {0};
        size_t bytesRead = recvfrom(
            chat_room_fd, 
            msg_buffer, 
            sizeof(msg_buffer), 
            0, 
            (struct sockaddr*) &client_address, 
            &client_address_size
        );

        std::stringstream stream(msg_buffer);
        lock.lock();
        incomming_messages.push(stream.str());
        lock.unlock();
        stream.clear();
    }
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
        std::cerr << "Bind failed\n" << std::endl;
        close(server_fd);
        return -1;
    }
    return 0;
}


/**
 * Sets up the server address for the given sockaddr_in structure.
 *
 *
 * @return 0 on success, -1 on failure.
 *
 * @throws None
 */
int setup_server_address(struct sockaddr_in& serv_addr, arg_type msg_type) {
    serv_addr.sin_family = AF_INET;

    if (msg_type == COMMAND) {
        serv_addr.sin_port = htons(PORT);
    }
    else if (msg_type == MSG) {
        serv_addr.sin_port = htons(CHAT_ROOM_PORT);
    }
    else if (msg_type == RECEIVE) {
        serv_addr.sin_port = htons(CHAT_ROOM_BRODCAST_PORT);
        // serv_addr.sin_addr.s_addr = INADDR_BROADCAST;
        // serv_addr.sin_port = htons(CHAT_ROOM_BRODCAST_PORT);
    }

    if (inet_pton(AF_INET, "10.183.70.66", &serv_addr.sin_addr) <= 0) {
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

    std::string filename = std::string("%PUT ") + filepath;
    send(sock, filename.c_str(), filename.size(), 0);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    char buffer[1024] = {0};
    while (size > 0) {
        file.read(buffer, std::min(size, (std::streamsize)sizeof(buffer)));
        send(sock, buffer, file.gcount(), 0);
        size -= file.gcount();
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
    std::string request = std::string("%GET ") + filename;
    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    std::ofstream outFile(filename, std::ios::binary);

    int bytesRead;
    std::cout << "Receiving file..." << std::endl;
    while ((bytesRead = read(sock, buffer, sizeof(buffer))) > 0) {
        std::cout << "File Name: " << filename << std::endl;
        outFile.write(buffer, bytesRead);
    }

    if (bytesRead < 0) {
        std::cerr << "Failed to receive file." << std::endl;
    } else {
        std::cout << "File received: " << filename << std::endl;
    }
}

/**
 *
 * @param sock The file descriptor of the socket to use for the file transfer.
 * @param msg The name of the file to be retrieved.
 *
 * @return None
 *
 * @throws None
 */
int send_command(int sock, const char* command, const char* filepath_or_filename) {
    if (strcmp(command, "%put") == 0) {
        send_put_request(sock, filepath_or_filename);
    } else if (strcmp(command, "%get") == 0) {
        send_get_request(sock, filepath_or_filename);
    } else {
        std::cerr << "Unknown command. Use 'put', 'get', or 'msg'." << std::endl;
    }

    close_socket(sock);
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
