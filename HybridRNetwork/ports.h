#ifndef PROTS_H_EXISTS
#define PROTS_H_EXISTS

/*
    Definitions for all ports used
*/
#define PORT 8080
#define CHAT_ROOM_PORT 8081
#define CHAT_ROOM_BRODCAST_PORT 8082

// P3
#define CONNECTION_STATUS_PORT 8083

// P4
#define RESOURCE_SHARE_PORT 8085

#define dprint(x) std::cout << "\n\n\t \033[32m" << x << "\033[0m\n\n";
#define greenPrint(x) "\033[32m" << x << "\033[0m"
#define redPrint(x) "\033[31m" << x << "\033[0m"
#endif