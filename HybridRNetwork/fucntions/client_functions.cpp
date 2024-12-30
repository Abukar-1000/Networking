#include "client_functions.h"
#include "../ports.h"


void handle_client(int new_socket, std::string msg) {
    char buffer[1024] = {0};
    if (msg.size() == 0){
        read(new_socket, buffer, 1024);
    }
    
    if (msg.size() > 0) {
        //add msg to the buffer
        strcat(buffer, msg.c_str());
    }
    std::string command(buffer);

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

    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
    }
    close_connection(new_socket);
}

void close_connection(int socket_fd) {
    close(socket_fd);
}

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

int start_listening(int server_fd) {
    if (listen(server_fd, 5) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return -1;
    }
    return 0;
}


int setup_server_address_dynamic(struct sockaddr_in& serv_addr, arg_type msg_type, std::string address) {
    serv_addr.sin_family = AF_INET;

    if (msg_type == COMMAND) {
        serv_addr.sin_port = htons(RESOURCE_SHARE_PORT);
    }
    else if (msg_type == MSG) {
        serv_addr.sin_port = htons(RESOURCE_SHARE_PORT);
    }

    if (inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        return -1;
    }
    return 0;
}


void send_get_request(int sock, std::string filename, std::string folder) {
    std::string request = std::string("%GET ") + filename;
    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    std::string path = folder + filename;
    std::ofstream outFile(path.c_str(), std::ios::binary);

    int bytesRead;
    while ((bytesRead = read(sock, buffer, sizeof(buffer))) > 0) {
        std::cout << "File Name: " << filename << std::endl;
        outFile.write(buffer, bytesRead);
        std::stringstream ss(buffer);
        dprint("Read in: " + ss.str())
    }

    if (bytesRead < 0) {
        std::cerr << "Failed to receive file." << std::endl;
    }

    if (outFile.is_open()) outFile.close();
}

std::string parseResourceUser(std::string line)
{
    const std::string flag = " (owned by ";
    size_t start = line.find(flag) + flag.length();
    size_t stop = line.find(")");
    return line.substr(start, line.length()).substr(0, (line.length() - start) - 1);
}

std::string parseResourceName(std::string line)
{
    const std::string flag = " (owned by ";
    size_t end = line.find(flag);
    return line.substr(0, end);
}

State getState(std::string line)
{
    size_t end = line.find("- Active");
    if (end == std::string::npos)
    {
        end = line.find("- Inactive");
    }
    std::string state = line.substr(end + 2, line.length());
    return (state == "Active")? Active: Offline;
}
 
std::string getUsername(std::string line)
{
    size_t end = line.find(" - Active");
    if (end == std::string::npos)
    {
        end = line.find(" - Inactive");
    }

    std::string name = line.substr(0, end);
    return name;
}

// modify to return map & table meta data
std::map<std::string, User> parseResources(std::string s)
{
    std::map<std::string, User> userResourceTable;
    std::map<std::string, State> userStateTable;
    std::stringstream ss(s);
    std::string line = "";
    bool parsingState = false;
    bool parsingResource = false;

    while (getline(ss, line))
    {
        parsingState = (
            line.find("Active") != std::string::npos ||
            line.find("Inactive") != std::string::npos
        );
        parsingResource = line.find(" (owned by ") != std::string::npos;
        bool skipLine = (
            line.find("Available Resources:") != std::string::npos ||
            line.find("Active Clients:") != std::string::npos
        );

        if (skipLine)
        {
            continue;
        }
        if (parsingState)
        {
            std::string username = getUsername(line);
            State state = getState(line);
            userStateTable[username] = state;
        }
        else if (parsingResource)
        {
            std::string resourceName = parseResourceName(line);
            std::string resourceUser = parseResourceUser(line);
            State state = userStateTable[resourceUser];
            User user {resourceUser, "", state};

            userResourceTable[resourceName] = user;
        }
    }
    return userResourceTable;
}

void displayResourceTable(ResourceTable table)
{
    const int columnWidth = 7;
    const int resourceWidth = 25;
    const int userWidth = 25;
    const int stateWidth = 15;
    const int separator = columnWidth + resourceWidth + userWidth + stateWidth + 5;
    
    std::cout << std::left;
    std::cout << std::setw(columnWidth) << "#" << std::setw(resourceWidth) << "Resource" << std::setw(userWidth) << "User" << std::setw(stateWidth) << "State" << std::endl;

    std::cout << std::string(separator, '-') << std::endl;
    size_t index = 0;
    for (
        auto it = table.begin();
        it != table.end();
        ++it
    ) {
        std::string resource = it->first;
        std::string username = it->second.name;
        std::cout << std::left;
        
        if (it->second.state == Active)
        {
            std::cout << std::setw(columnWidth) << index + 1 
                    << std::setw(resourceWidth) << resource 
                    << std::setw(userWidth) << username 
                    << std::setw(stateWidth) << greenPrint("Active") << std::endl;
        }
        else if (it->second.state == Offline)
        {
            std::cout << std::setw(columnWidth) << index + 1 
                    << std::setw(resourceWidth) << resource 
                    << std::setw(userWidth) << username 
                    << std::setw(stateWidth) << redPrint("Inactive") << std::endl;
        }
        ++index;
    }
}

MessageType identify(std::stack<std::string>& s)
{
    if (isAuthenticationMsg(s))
    {
        return Authentication;
    }
    else if (isResourceIpMsg(s))
    {
        return ResourceRequest;
    }
    else if (
     !isAuthenticationMsg(s) &&
     !isResourceIpMsg(s)
    ) 
    {
        return List;
    }
}

bool isResourceIpMsg(std::stack<std::string>& s)
{
    std::string msg = s.top();
    const std::string flag = "RESOURCEIP|";
    return msg.find(flag) != std::string::npos;
}

bool isAuthenticationMsg(std::stack<std::string>& s)
{
    std::string msg = s.top();
    return (
        msg == "Registration successful" ||
        msg == "Username already exists" ||
        msg == "Invalid registration format" || 
        msg == "Login successful" ||
        msg == "Password incorrect" ||
        msg == "Auto-registered and logged in"
    );
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
