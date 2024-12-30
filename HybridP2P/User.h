#ifndef USER_H
#define USER_H

#include <string>
#include <ctime>

class User {
private:
    std::string username;
    std::string password; 
    std::string clientIP;
    int port;
    bool activeStatus;
    time_t lastHeartbeat;

public:
    User(const std::string& username, 
         const std::string& password,
         const std::string& clientIP, 
         int port)
        : username(username)
        , password(password)
        , clientIP(clientIP)
        , port(port)
        , activeStatus(true)
        , lastHeartbeat(time(nullptr))
    {}

    // Getters
    std::string getUsername() const { return username; }
    std::string getPassword() const { return password; }
    std::string getClientIP() const { return clientIP; }
    int getPort() const { return port; }
    bool isActive() const { 
        time_t now = time(nullptr);
        return activeStatus && (now - lastHeartbeat < 30);  // 30 seconds timeout
    }

    // Setters
    void setActiveStatus(bool status) {
        activeStatus = status;
        if (status) {
            lastHeartbeat = time(nullptr);
        }
    }

    void updateHeartbeat() {
        lastHeartbeat = time(nullptr);
    }

    // Add these new methods
    time_t getLastActiveTime() const {
        return lastHeartbeat;
    }

    void setClientIP(const std::string& ip) {
        clientIP = ip;
    }

    void setPort(int p) {
        port = p;
    }

    void updateLastActiveTime(time_t time) {
        lastHeartbeat = time;
    }
};

#endif // USER_H