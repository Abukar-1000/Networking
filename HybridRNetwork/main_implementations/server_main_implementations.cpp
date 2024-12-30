#include "../fucntions/server_functions.h"
#include "server_main_implementations.h"

int project2() {
    /*
        Create chat app & use a thread to handle commands in the 
        background
    */
    std::thread command_handler_thread(handle_command_request);
    command_handler_thread.detach();

    test_msgs();
    return 0;
}
