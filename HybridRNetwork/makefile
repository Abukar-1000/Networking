server: server.cpp server_main_implementations User Resource
	g++ -std=c++17 server_functions.o User.o Resource.o server_main_implementations.o server.cpp -o server -pthread
	./server

Resource: Resource.h
	g++ -std=c++17 -g -c Resource.h -o Resource.o

User: User.h
	g++ -std=c++17 -g -c User.h -o User.o

server_main_implementations: main_implementations/server_main_implementations.h main_implementations/server_main_implementations.cpp server_functions
	g++ -std=c++17 -g -c server_functions.o main_implementations/server_main_implementations.cpp -o server_main_implementations.o

server_functions: fucntions/server_functions.h fucntions/server_functions.cpp
	g++ -std=c++17 -g -c fucntions/server_functions.cpp -o server_functions.o



client_functions: fucntions/client_functions.h fucntions/client_functions.cpp
	g++ -std=c++17 -g -c fucntions/client_functions.cpp -o client_functions.o

client_main_implementations: main_implementations/client_main_implementations.h main_implementations/client_main_implementations.cpp client_functions
	g++ -std=c++17 -g -c client_functions.o main_implementations/client_main_implementations.cpp -o client_main_implementations.o

client: client.cpp client_functions client_main_implementations
	g++ -std=c++17 client_functions.o client_main_implementations.o client.cpp -o client

clean:
	rm *.o
	rm client
	make client
	clear