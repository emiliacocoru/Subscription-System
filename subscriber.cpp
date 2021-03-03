#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <vector>
#include <netinet/tcp.h> 

#define BUFLEN 2000

using namespace std;

int main(int argc, char *argv[]) {
	int socket_server, n, ret;
	struct sockaddr_in addr_server;
	char buffer[BUFLEN];
	fd_set read_fds;
	fd_set tmp_fds;

	int fdmax;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);


	// ./subscriber <ID_Client> <IP_Server> <Port_Server>
	if (argc < 4) {
		perror("[ERROR] Try again! Usage: ./subscriber <ID_CLIENT> <IP_Server> <Port_Server>\n");
		return 0;
	}
	// sochet TCP pentru conectarea la server
	socket_server = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_server < 0) {
		printf("[ERROR] Failed to open socket TCP!");
	}

	addr_server.sin_family = AF_INET;
	addr_server.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &addr_server.sin_addr);
	if (ret < 0) {
		perror("[ERROR] inet_aton is not working properly!");
	}

	FD_SET(socket_server, &read_fds);
	fdmax = socket_server;
	FD_SET(0, &read_fds);
	// conexiunea catre server
	ret = connect(socket_server, (struct sockaddr *) &addr_server, sizeof(addr_server));
	if (ret < 0) {
		perror("[ERROR] Failed to connect!");
	}

	// trimit ip-ul serverului
	ret = send(socket_server, argv[1], strlen(argv[1]), 0);
	if (ret < 0) {
		perror("[ERROR] Failed to send IP to server");
	}
	memset(buffer, 0, BUFLEN);
	// așteaptă confirmare
    ret = recv(socket_server, buffer, BUFLEN, 0);
    if (ret < 0) {
    	perror("[ERROR] Failed to received confirmation about IP from server");
    }
    // afișează eroare și iese
	if (buffer[0] == 'i') {
		perror("[ERROR] ID already used");
		return 0;
	}    

    // se dezactivează altoritmul lui Nagle
	int flag = 1;
	setsockopt(socket_server, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	if (flag < 0) {
		perror("[ERROR] Failed to disable Nagle's algorithm!");
	}

	int exit_time = 1;
	while(exit_time == 1) {
		tmp_fds = read_fds;
		// selectare sockeți
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if (ret < 0) {
			perror("[ERROR] Failed to select!");
		}
		if (FD_ISSET(0, &tmp_fds)) {
			// are loc citirea de la tastatură
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);

			// îmi impart ca și în server ce primesc de la tastatură
			// ca elemente separate ale unui vector
			vector<string> tcp_request;
			string message = buffer;
			int begin = 0;
			int word_size = 0;
			int words = 0;
			int len = strlen(buffer);
			for (int i = 0; i < len; i++) {
				if (message[i] == ' ') {
					tcp_request.push_back(message.substr(begin, word_size));
					word_size = 0;
					begin = i + 1;
					words++;
				} else {
					word_size++;
				}
			}	
			if (words == 0) {
				tcp_request.push_back(message.substr(0, word_size - 1));
			}
			words++;

			// verific pentru fiecare in parte
			// daca se respecta conditia 
			if (tcp_request[0] == "subscribe") {
				if (words == 3) {
					// in caz afirmativ trimit mai departe
					ret = send(socket_server, buffer, BUFLEN, 0);
					if (ret < 0) {
						perror("[ERROR] Failed to send to server the subscribe command!");
					}
				} else {
					perror("[ERROR]Command is wrong.Try again: subscribe <topic> <SF>!");
				}
			} else if (tcp_request[0] == "unsubscribe") {
				if (words == 2) {
					ret = send(socket_server, buffer, BUFLEN, 0);
					if (ret < 0) {
						perror("[ERROR] Failed to send to server the unsubscribe command!");
					}
				} else {
					perror("[ERROR]Command is wrong.Try again: unsubscribe <topic>");
				}
			} else if (tcp_request[0] == "exit") {
				if (words == 1) {
					ret = send(socket_server, buffer, BUFLEN, 0);
					if (ret < 0) {
						perror("[ERROR] Faled to send to server the exit command");
					} 
					exit_time = 0;

				} else {
					perror("[ERROR]Command is wrong.Try again: exit");
				}
			} else {
				perror("[ERROR] Command does not exist. Maybe you want to use : subscribe <topic> <SF>, unsubscribe <topic> or exit");
			}

		} else {
			// dacă am primit un mesaj de la server
			memset(buffer, 0, BUFLEN);
			n = recv(socket_server, buffer, BUFLEN, 0);
			if (n < 0) {
				perror("[ERROR] Failed to received!");
			} else {
				// verific dacă este de eroare
				if (buffer[0] == 't') {
					perror("[ERROR] The topic doesn't exist!");
				} else if (buffer[0] == 'u') {
					perror( "[ERROR] You are not subscribe at this topic in first place!");
				}else if (buffer[0] == 'i') {
					return 0;
				} else if (buffer[0] != ' ') {
					// sau mesajul propriu-zis
					cout << buffer;
				}
			}
		}		
	}
	close(socket_server);
	return 0;
}
