#include "../ports.h"
#include "../fucntions/client_functions.h"
#include "client_main_implementations.h"

int project3(int argc, char const *argv[]) {
    // project 3 code
    return 0;
}


int project2(int argc, char const *argv[]) {
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