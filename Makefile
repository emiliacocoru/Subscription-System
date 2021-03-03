all: server subscriber

server: server.cpp
	g++ -Wall -Wextra -g -std=c++11 server.cpp -o server

subscriber: subscriber.cpp
	g++ -Wall -Wextra -g -std=c++11 subscriber.cpp -o subscriber

clean: 
	rm -f server subscriber *.txt