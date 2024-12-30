#include "main_implementations/server_main_implementations.h"
// server.cpp
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <ctime>
#include <mutex>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "User.h"
#include "Resource.h"
#include "ports.h"
#include <sstream>

class P2PServer {
private:
    std::string serverIP; // remove
    std::vector<User> clients;
    std::vector<Resource> resources;
    std::mutex clientsMutex;
    std::mutex resourcesMutex;
    int serverSocket;
    int heartbeatSocket;
    bool running;
    
    void checkClientLiveness() {
        // struct sockaddr_in heartbeatAddr;
        int heartbeatSocket = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in heartbeatAddr;
        heartbeatAddr.sin_family = AF_INET;
        heartbeatAddr.sin_port = htons(CONNECTION_STATUS_PORT);
        inet_pton(AF_INET, serverIP.c_str(), &heartbeatAddr.sin_addr);

        /**
         * Timeout options for recieving an Heartbeat ack
         *  - Must respond within 2 seconds 
        */

       // https://stackoverflow.com/questions/15941005/making-recvfrom-function-non-blocking
       struct timeval heartbeatTimeoutOptions;
       heartbeatTimeoutOptions.tv_sec = 2;
       heartbeatTimeoutOptions.tv_usec = 0;

        const char* FAILED_ACK_COLOR = "\033[95m";
        // We wait at most 2 seconds to recieve ack from client
        // https://stackoverflow.com/questions/21515946/what-is-sol-socket-used-for
        setsockopt(heartbeatSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &heartbeatTimeoutOptions, sizeof(heartbeatTimeoutOptions));
        char * ACK = "HEARTBEAT_ACK";

        while(running) {
            char buffer[1024];
            std::cout << "getting heartbeat lock " << "clients: " << this->clients.size() << "\n";
            this->clientsMutex.lock();
            std::cout << "got client lock\n\n";
            for(auto& client : clients) {
                client.setActiveStatus(false);
                // Send UDP heartbeat message to client
                struct sockaddr_in clientAddr;
                clientAddr.sin_family = AF_INET;
                clientAddr.sin_port = htons(CONNECTION_STATUS_PORT);
                socklen_t clientAddressSize = sizeof(clientAddr);
                inet_pton(AF_INET, client.getClientIP().c_str(), &clientAddr.sin_addr);
                
                char heartbeat[] = "HEARTBEAT";
                sendto(
                    heartbeatSocket, 
                    heartbeat, 
                    strlen(heartbeat), 
                    0,
                    (struct sockaddr*)&clientAddr, 
                    sizeof(clientAddr)
                );

                std::cout << "Server: Sent heartbeat to " << client.getUsername() 
                            << " IP: " + client.getClientIP() 
                            << " Port: " << CONNECTION_STATUS_PORT
                            << std::endl;

                this->clientsMutex.unlock();
                std::cout << "relinquished client lock\n";
                sleep(2);

                this->clientsMutex.lock();
                std::cout << "reaquired client lock\n";
                // std::stringstream ss(buffer);
                // memset(buffer, 0, 1024); // clear buffer

                if (client.isActive() == false) {
                    std::cout << FAILED_ACK_COLOR << "Client " << client.getUsername() << " is now inactive" << "\n\n";
                }

                std::cout << " \033[32m";
            }
            this->clientsMutex.unlock();
            sleep(7);
        }
    }

    std::string getClientList() {
        std::string list = "Active Clients:\n";
        // std::lock_guard<std::mutex> lock(clientsMutex);
        for(const auto& client : clients) {
            list += client.getUsername() + " - " + 
                    (client.isActive() ? "Active" : "Inactive") + "\n";
        }
        return list;
    }

    std::string getResourceList() {
        std::string list = "Available Resources:\n";
        
        this->clientsMutex.lock();
        for(const auto& resource : resources) {
            list += resource.getResourceName() + " (owned by " + 
                    resource.getOwnerName() + ")\n";
        }
        this->clientsMutex.unlock();
        return list;
    }

    void handleConnections() {
        struct sockaddr_in serverAddr;
        serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
        
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);  // Default port
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        
        bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        
        while(running) {
            char buffer[1024];
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            std::cout << "Server: Waiting for incoming messages..." << std::endl;
            
            int n = recvfrom(serverSocket, buffer, 1024, 0,
                           (struct sockaddr*)&clientAddr, &clientLen);
            buffer[n] = '\0';
            
            std::string message(buffer);
            std::cout << "Server: Received message: " << message << std::endl;
            
            if(message.substr(0, 8) == "REGISTER") {
                // Expected format: "REGISTER|username|password"
                size_t firstDelim = message.find('|');
                size_t secondDelim = message.find('|', firstDelim + 1);
                
                if (firstDelim != std::string::npos && secondDelim != std::string::npos) {
                    std::string username = message.substr(firstDelim + 1, secondDelim - firstDelim - 1);
                    std::string password = message.substr(secondDelim + 1);
                    
                    std::string clientIP = inet_ntoa(clientAddr.sin_addr);
                    int clientPort = ntohs(clientAddr.sin_port);
                    
                    // Check if user already exists
                    bool userExists = false;
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        for (const auto& client : clients) {
                            if (client.getUsername() == username) {
                                userExists = true;
                                break;
                            }
                        }
                        
                        // Add new user if they don't exist
                        if (!userExists) {
                            clients.emplace_back(username, password, clientIP, clientPort);
                            std::string response = "Registration successful";
                            sendto(serverSocket, response.c_str(), response.length(), 0,
                                  (struct sockaddr*)&clientAddr, clientLen);
                        } else {
                            std::string response = "Username already exists";
                            sendto(serverSocket, response.c_str(), response.length(), 0,
                                  (struct sockaddr*)&clientAddr, clientLen);
                        }
                    }
                } else {
                    std::string response = "Invalid registration format";
                    sendto(serverSocket, response.c_str(), response.length(), 0,
                          (struct sockaddr*)&clientAddr, clientLen);
                }
            }
            else if(message.substr(0, 5) == "LOGIN") {
                // Expected format: "LOGIN|username|password"
                std::cout << "Trying login for client" << std::endl;
                size_t firstDelim = message.find('|');
                size_t secondDelim = message.find('|', firstDelim + 1);
                
                if (firstDelim != std::string::npos && secondDelim != std::string::npos) {
                    std::string username = message.substr(firstDelim + 1, secondDelim - firstDelim - 1);
                    std::string password = message.substr(secondDelim + 1);
                    std::string clientIP(inet_ntoa(clientAddr.sin_addr));
                    int clientPort = ntohs(clientAddr.sin_port);
                    std::cout << "og IP: " << inet_ntoa(clientAddr.sin_addr) << "\n";
                    std::cout << "Client IP: " << clientIP << ", Client Port: " << clientPort << std::endl;
                    // std::lock_guard<std::mutex> lock(clientsMutex);
                    bool userFound = false;
                    std::cout << "Checking credentials for user: " << username << std::endl;
                    
                    // First try to login
                    for (auto& client : clients) {
                        std::cout << "Comparing with existing user: " << client.getUsername() << std::endl;
                        if (client.getUsername() == username) {
                            userFound = true;
                            if (client.getPassword() == password) {
                                client.setClientIP(clientIP);
                                client.setPort(clientPort);
                                client.setActiveStatus(true);
                                client.updateLastActiveTime(time(nullptr));
                                sendto(serverSocket, "Login successful", 16, 0,
                                      (struct sockaddr*)&clientAddr, clientLen);
                            } else {
                                sendto(serverSocket, "Password incorrect", 17, 0,
                                      (struct sockaddr*)&clientAddr, clientLen);
                            }
                            break;
                        }
                    }
                    std::cout << "User found: " << userFound << std::endl;
                    // If user doesn't exist, register them
                    if (!userFound) {
                        clients.emplace_back(username, password, clientIP, clientPort);
                        std::string response = "Auto-registered and logged in";
                        std::cout << "Auto-registered and logged in" << std::endl;
                        sendto(serverSocket, response.c_str(), response.length(), 0,
                              (struct sockaddr*)&clientAddr, clientLen);
                    }
                }
            }
            else if(message.substr(0, 4) == "LIST") {
                // Send client and resource lists
                std::string response = getClientList() + "\n" + getResourceList();
                sendto(serverSocket, response.c_str(), response.length(), 0,
                      (struct sockaddr*)&clientAddr, clientLen);
            }
            // is implemented in handle 
            else if(message == "HEARTBEAT_ACK") {
                this->clientsMutex.lock();
                // Update client's active status
                std::string clientIP = inet_ntoa(clientAddr.sin_addr);
                int clientPort = ntohs(clientAddr.sin_port);
                std::cout << "updating status for client " << clientIP << ":" << clientPort << "\n";
                for(auto& client : clients) {
                    if(client.getClientIP() == clientIP) {
                        client.setActiveStatus(true);
                        const char* SUCCESS_ACK_COLOR = "\033[92m";
                        std::cout << SUCCESS_ACK_COLOR << "Client " << client.getUsername() << " is active" << "\n\n";
                        client.updateLastActiveTime(time(nullptr));
                        this->clientsMutex.unlock();
                        break;
                    }
                }
            }
        }
    }

public:
    P2PServer() : 
        running(true),
        clientsMutex(),
        resourcesMutex(),
        serverIP("10.0.0.129")
    {
        // Start the liveness checking thread
        std::thread livenessThread(&P2PServer::checkClientLiveness, this);
        livenessThread.detach();
        
        // Start the connection handling thread
        std::thread connectionThread(&P2PServer::handleConnections, this);
        // add some dummy users
        // clients.emplace_back("ndhoka", "1234", "127.0.0.1", 8080);
        // clients.emplace_back("abkitch", "5678", "127.0.0.1", 8080);
        connectionThread.detach();
    }
    
    void addResource(const std::string& name, const std::string& owner) {
        std::lock_guard<std::mutex> lock(resourcesMutex);
        resources.emplace_back(name, owner);
    }
    
    ~P2PServer() {
        running = false;
        close(serverSocket);
    }
};

int main() {
    try {
        std::cout << "Starting P2P Server..." << std::endl;
        P2PServer server;


        
        std::cout << "Server is running. Press Enter to shutdown." << std::endl;
        std::cin.get();  // Wait for Enter key
        
        std::cout << "Shutting down server..." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}