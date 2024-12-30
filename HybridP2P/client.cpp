
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


void sendToServer(const std::string& message, int clientSocket, struct sockaddr_in& serverAddr) {
    std::cout << "Client: Sending message: " << message << std::endl;
    int result = sendto(clientSocket, message.c_str(), message.length(), 0,
            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result < 0) {
        std::cerr << "Client: Failed to send message. Error: " << strerror(errno) << std::endl;
    }
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
    std::string loginMsg = "LOGIN|" + username + "|" + password;
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

void requestList(int clientSocket, struct sockaddr_in& serverAddr) {
    std::string listMsg = "LIST";
    sendto(clientSocket, listMsg.c_str(), listMsg.length(), 0,
            (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    // Wait for response
    char buffer[1024] = {0};
    struct sockaddr_in responseAddr;
    socklen_t responseLen = sizeof(responseAddr);
    
    int n = recvfrom(clientSocket, buffer, 1024, 0,
                    (struct sockaddr*)&responseAddr, &responseLen);
    
    buffer[n] = '\0';
    
    std::cout << "Received list from server:\n" << buffer << std::endl;
}


// Example main function showing usage
int main() {

    bool running = true;
    std::stack<std::string> messages;
    std::mutex messageLock;
    std::string serverIP = "10.0.0.129";

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
    
    messageThread.detach();
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
            std::cout << "2. Exit\n";
            std::cout << "Choice: ";

            // std::string input = future.get();
            std::string input = "";
            std::getline(std::cin, input);
            int choice = std::stoi(input);
            
            switch (choice) {
                case 1:
                    requestList(clientSocket, serverAddr);
                    break;
                case 2:
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