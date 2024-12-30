// Class to store resource information
#ifndef RESOURCE_EXISTS
#define RESOURCE_EXISTS
#include <iostream>

class Resource {
private:
    std::string resource_name;
    std::string owner_name;
    
public:
    Resource(void) {}
    Resource(std::string name, std::string owner) : 
        resource_name(name), owner_name(owner) {}

    std::string getResourceName() const { return resource_name; }
    std::string getOwnerName() const { return owner_name; }
};
#endif