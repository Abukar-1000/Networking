
#include <iostream>
#include "main_implementations/client_main_implementations.h"

#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include "ports.h"
#include "fucntions/client_functions.h"

std::string parsePath(std::string path, std::string folder)
{
    const int fileNameStart = path.find(folder) + folder.length();
    std::string file = path.substr(fileNameStart + 1, path.length());
    return file;
}

std::string getResources(std::string folder)
{
    std::string files;
    std::string delimiter = ",";
    std::string path = "./" + folder;

    for (const auto & entry : std::filesystem::directory_iterator(path)) 
    {
        std::string file = parsePath(entry.path(), folder);
        files += file + delimiter;
    }

    files = files.substr(0, files.length() - 1);
    return files;
}

void sendToServer(const std::string& message, int clientSocket, struct sockaddr_in& serverAddr) {
    int result = sendto(clientSocket, message.c_str(), message.length(), 0,
            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0) {
        std::cerr << "Client: Failed to send message. Error: " << strerror(errno) << std::endl;
    }
}

void getResource(
    bool& running,
    std::mutex& requestMutex,
    std::queue<Resolve::ResourceRequest>& requestQueue
) {
    while (running)
    {
        if (requestMutex.try_lock())
        {
            if (requestQueue.size() > 0)
            {
                Resolve::ResourceRequest request = requestQueue.front();
                int socket = create_socket(COMMAND);
                
                if (socket < 0) return;

                struct sockaddr_in serv_addr;
                int state = setup_server_address_dynamic(serv_addr, COMMAND, request.ipAddress);
                if (state < 0) return;

                if (connect_to_server(socket, serv_addr) < 0) return;
                
                send_get_request(socket, request.resource, "recieved/");
                requestQueue.pop();
            }
            requestMutex.unlock();
        }
        sleep(THREAD_WAIT + 3);
    }
}

void sendResource(bool& running) {
    int server_fd = create_socket(COMMAND);
    if (server_fd < 0) return;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(RESOURCE_SHARE_PORT);

    if (bind_socket(server_fd, address) < 0) return;
    if (start_listening(server_fd) < 0) return;

    std::cout << "P2P Server is waiting for a connection..." << std::endl;

    while (running) {
        int new_socket = accept_connection(server_fd, address);
        if (new_socket < 0) return;

        handle_client(new_socket, "");
        // !create a new thread for each client and detach it
    }

    close_connection(server_fd);
    return;
}

void handleHeartbeat(int clientSocket, struct sockaddr_in& serverAddr, bool& running) {
    try {

        struct sockaddr_in heartbeatAddr;
        int heartbeatSocket = socket(AF_INET, SOCK_DGRAM, 0);

        if (heartbeatSocket < 0) {
            std::cout << "failed to create hearbeat socket\n";
        }
        heartbeatAddr.sin_family = AF_INET;
        heartbeatAddr.sin_addr.s_addr = INADDR_ANY;
        heartbeatAddr.sin_port = htons(CONNECTION_STATUS_PORT);

        if (bind_socket(heartbeatSocket, heartbeatAddr) < 0) {
            std::cout << "Heartbeat failed to bind\n";
            return;
        };

        while (running) {
            char buffer[1024];
            struct sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);
            
            // Non-blocking receive to check for heartbeat
            int n = recvfrom(heartbeatSocket, buffer, 1024, 0,
                            (struct sockaddr*)&senderAddr, &senderLen);

            if (n > 0) {
                buffer[n] = '\0';
                std::string message(buffer);
                
                if (message == "HEARTBEAT") {
                    // Send acknowledgment back to server
                    std::string ack = "HEARTBEAT_ACK";
                    sendto(heartbeatSocket, ack.c_str(), ack.length(), 0,
                            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (std::exception& e) {
        running = false;
        std::cout << "Heartbeat failed\n\n";
        std::cout << e.what() << "\n";
    } catch (...) {
        std::cout << "Heartbeat handler failed\n\n";
    }
}

void handleMessages(bool& running, int clientSocket, std::stack<std::string>& messages, std::mutex& messageLock) {
    try {

        while (running) {
            char buffer[1024];
            struct sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);
            
            size_t n = recvfrom(clientSocket, buffer, 1024, 0,
                            (struct sockaddr*)&senderAddr, &senderLen);
        
            messageLock.lock();
            buffer[n] = '\0';
            std::string message(buffer);
            
            messages.push(message);
            messageLock.unlock();
        }
    }
    catch (std::exception& e) {
        running = false;
        std::cout << "message handler failed\n\n";
        std::cout << e.what() << "\n";
    } catch (...) {
        std::cout << "message handler failed\n\n";
    }
}

void registerWithServer(
    std::string username, 
    std::string password, 
    int clientSocket, 
    struct sockaddr_in& serverAddr, 
    std::stack<std::string>& messages, 
    std::mutex& messageLock
) {
    // First try to login
    // Get available resources
    std::string resourceFolder = "files";
    std::string resources = getResources(resourceFolder);

    std::string loginMsg = "LOGIN|" + username + "|" + password + "|" + resources;
    sendToServer(loginMsg, clientSocket, serverAddr);

    messageLock.lock();
    // Havent recieved response from server
    if (messages.size() < 1) {
        messageLock.unlock();
        int count = 1;
        bool waiting = true;
        while (waiting) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << count * 2 << "\n";
            messageLock.lock();
            if (messages.size() > 0) {
                waiting = false;
            }
            else {
                messageLock.unlock();
            }
        }
    }

    std::string response = messages.top();
    if (response == "Login successful") {
        std::cout << "Login successful!\n";
    }
    else if (response == "Auto-registered and logged in" || response == "HEARTBEAT") {
        std::cout << "Registration successful!\n";
    }
    else if (response == "User exists, password incorrect") {
        std::cout << "Authentication failed: " << response << "\n";
    }
    else if (response == "User not found") {
        // Try to register
        std::string registerMsg = "REGISTER|" + username + "|" + password;
        
        messageLock.unlock();
        sendToServer(registerMsg, clientSocket, serverAddr);
        
        messageLock.lock();
        response = messages.top();
        
        if (response == "Registration successful") {
            std::cout << "Registration successful!\n";
        } else {
            std::cout << "Registration failed: " << response << "\n";
        }
    }

    messageLock.unlock();
}

ResourceTable requestList(
    int clientSocket, 
    struct sockaddr_in& serverAddr,
    std::stack<std::string>& messages, 
    std::mutex& messageLock
) {
    std::string listMsg = "LIST";

    // remove following comments
    // while (!messageLock.try_lock()) {}

    sendto(clientSocket, listMsg.c_str(), listMsg.length(), 0,
            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    // messageLock.unlock();


    // Wait for response
    char buffer[1024] = {0};
    struct sockaddr_in responseAddr;
    socklen_t responseLen = sizeof(responseAddr);

    while (
        !messageLock.try_lock() ||
        identify(messages) != List
    ) {
        messageLock.unlock();
        sleep(THREAD_WAIT);
    }
    
    ResourceTable table = parseResources(messages.top());
    messageLock.unlock();

    return table;
}

Request::ResourceRequest getUserChoice(ResourceTable table)
{
    std::cout << "Enter the number corrensponding to your selection:\t";
    std::string input = "";
    std::getline(std::cin, input);
    int choice = std::stoi(input) - 1;

    bool isValidChoice = choice < table.size();
    while (!isValidChoice)
    {
        displayResourceTable(table);
        std::cout << redPrint("Invalid choice ") << "please try again:\n";

        input = "";
        std::getline(std::cin, input);
        choice = std::stoi(input) - 1;
        isValidChoice = choice < table.size();
    }

    int counter = 0;
    for (
        auto it = table.begin();
        it != table.end();
        ++it
    )
    {
        if (counter == choice)
        {
            return Request::ResourceRequest(
                it->first,
                it->second.name
            );
        }
        ++counter;
    }
    
}

std::string requestResource(
    ResourceTable table,
    int clientSocket,
    struct sockaddr_in& serverAddr,
    std::stack<std::string>& messages, 
    std::mutex& messageLock,
    std::queue<Resolve::ResourceRequest>& requests,
    std::mutex& fileRequestMutex
)
{
    Request::ResourceRequest req = getUserChoice(table);
    std::string resourceMsg = "RESOURCE|" + req.username + "|" + req.resource;

    sendto(clientSocket, resourceMsg.c_str(), resourceMsg.length(), 0,
            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    // update check for resource res
    while (
        !messageLock.try_lock() ||
        identify(messages) != ResourceRequest
    ) {
        messageLock.unlock();
        sleep(THREAD_WAIT + 1);
    }

    std::string response = messages.top();
    size_t firstDelim = response.find('|');
    // size_t secondDelim = response.find('|', firstDelim + 1);
    
    while(!fileRequestMutex.try_lock())
    {
        sleep(THREAD_WAIT + 1);
    }

    if (firstDelim != std::string::npos) {
        std::string ipAddress = response.substr(firstDelim + 1);
        Resolve::ResourceRequest fileReq(req.resource, ipAddress);
        requests.push(fileReq);
        // pass this into stack for another thread to handle
        dprint("Sending TCP Request to: " + ipAddress)
        dprint("Requesting " + req.resource + " from " + req.username + "\n")
    }

    fileRequestMutex.unlock();
    messageLock.unlock();
    // make call to other client using TCP [Abstract]
    return "Requesting " + req.resource + " from " + req.username + "\n";
}
// Example main function showing usage
int main() {

    bool running = true;
    std::mutex messageLock;
    std::mutex requestMutex;
    ResourceTable userResourceTable;
    std::stack<std::string> messages;
    std::string serverIP = "10.0.0.129";
    std::queue<Resolve::ResourceRequest> requestQueue;

    // Create UDP socket
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        std::cout << "failed to create client socket\n\n";
    }
    // Configure server address
    struct sockaddr_in serverAddr, heartbeatAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    

    std::cout << "Welcome to the P2P Client!\n";
    std::string username, password;
    std::cout << "Enter username: ";
    std::getline(std::cin, username);
    std::cout << "Enter password: ";
    std::getline(std::cin, password);
    

    // Start heartbeat handling thread
    std::thread heartbeatThread(
        handleHeartbeat,
        clientSocket, 
        std::ref(serverAddr),
        std::ref(running)
    );
    
    // Start message handling thread
    std::thread messageThread(
        handleMessages, 
        std::ref(running),
        clientSocket,
        std::ref(messages),
        std::ref(messageLock)
    );

    // Start file sharing thread
    std::thread sendFileThread(
        sendResource, 
        std::ref(running)
    );

    // Start file request thread
    std::thread requestFileThread(
        getResource, 
        std::ref(running),
        std::ref(requestMutex),
        std::ref(requestQueue)
    );

    
    messageThread.detach();
    sendFileThread.detach();
    requestFileThread.detach();
    heartbeatThread.detach();

    std::cout << "Authenticating: " << username << std::endl;
    registerWithServer(
        username,
        password,
        clientSocket,
        serverAddr,
        messages,
        messageLock
    );

    try {

        while (running) {
            std::cout << "\nCommands:\n";
            std::cout << "1. Request list\n";
            std::cout << "2. Request file\n";
            std::cout << "3. Exit\n";
            std::cout << "Choice: ";

            std::string input = "";
            std::getline(std::cin, input);
            int choice = std::stoi(input);
            
            switch (choice) {
                case 1:
                    userResourceTable = requestList(
                        clientSocket, 
                        serverAddr,
                        messages, 
                        messageLock
                    );
                    displayResourceTable(userResourceTable);
                    break;
                
                case 2:
                    userResourceTable = requestList(
                        clientSocket, 
                        serverAddr,
                        messages, 
                        messageLock
                    );
                    displayResourceTable(userResourceTable);
                    requestResource(
                        userResourceTable,
                        clientSocket, 
                        serverAddr,
                        messages, 
                        messageLock,
                        requestQueue,
                        requestMutex
                    );
                    break;
                case 3:
                    return 0;
                default:
                    std::cout << "Invalid choice\n";
            }
        }
        
    } catch (std::exception& e) {
        running = false;
        std::cout << "main failed\n\n";
        std::cout << e.what() << "\n";
    } catch (...) {
        std::cout << "main failed\n\n";
    }

    running = false;
    return 0;
}