// Class to store resource information
#ifndef RESOURCE_EXISTS
#define RESOURCE_EXISTS
#include <iostream>

class Resource {
private:
    std::string resource_name;
    std::string owner_name;
    std::string owner_IP;
    
public:
    Resource(void) {}
    Resource(
        std::string name, 
        std::string owner,
        std::string owner_ip
    ) : 
        resource_name(name), 
        owner_name(owner),
        owner_IP(owner_ip)
    {}

    std::string getResourceName() const { return resource_name; }
    std::string getOwnerName() const { return owner_name; }
    std::string getOwnerIP() const { return owner_IP; }
};
#endif