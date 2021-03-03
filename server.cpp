#include <iostream> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <fstream>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>


#define BUFLEN 2000
#define BUFFLEN 1500
#define MAX 300
using namespace std;


int main(int argc, char *argv[]) {
	int socket_tcp, socket_udp;
	int new_socket;
	unsigned short port;
	char buffer[BUFLEN];
	struct sockaddr_in address_tcp, address_udp;
	int exit = 0;

	// <topic, vector de id>
	unordered_map<string, vector<string>> topic_with_clients;
	// <id client, vectori cu topicurile la care e this_client>
	unordered_map<string, vector<string>> permanent;
	// <id client, dacă este online sau nu
	// 1 = online
	// 0 = offline
	unordered_map<string, int> status_client;
	// <socket, id client>
	unordered_map<int, string> find_id;
	// <id client, socket>
	unordered_map<string, int> find_socket;

	int n, ret;
	socklen_t size_address = sizeof(struct sockaddr_in);

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	// se golește mulțimea de descriptori de citire(read_fds) și mulțimea temporala (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);


	if (argc > 2 || argc < 2) {
		perror("[ERROR] Try again. Usage : ./server PORT");
		return 0;
	}
	port = atoi(argv[1]);
	if (port == 0) {
		perror("[ERROR] Atoi didt'n work.Incorrect port number in server");
	}

	// socket udp pentru receptionarea conexiunilor
	socket_udp =  socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_udp < 0) {
		perror("[ERROR]  Failed to create socket UDP in Server!");
	}
	// inițializare address_udp
	address_udp.sin_family = AF_INET; // protocol IPv4
	address_udp.sin_port = htons(atoi(argv[1]));  // port
	address_udp.sin_addr.s_addr = INADDR_ANY;

	// bind UDP
	ret = bind(socket_udp, (struct sockaddr *) &address_udp, sizeof(struct sockaddr_in));
	if (ret < 0) {
		perror("[ERROR] Failed to bind for UDP in server");
	}


	// socket TCP pentru recepționarea conexiunilor
	socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_tcp < 0) {
		perror("[ERROR] Failed to create socket TCP in Server!");
	}

	// inițializare address_tcp
	memset((char *) &address_tcp, 0, sizeof(address_tcp));
	address_tcp.sin_family = AF_INET;
	address_tcp.sin_port = htons(port);
	address_tcp.sin_addr.s_addr = INADDR_ANY;

	// bind TCP
	ret = bind(socket_tcp, (struct sockaddr*) &address_tcp, sizeof(struct sockaddr_in));
	if (ret < 0) {
		perror("[ERROR] Failed to bind for TCP in server");
	}

	// ascultă la clientul TCP
	ret = listen(socket_tcp, MAX);

	// se adaugă noi file descriptori în mulțimea read_fds
	FD_SET(socket_tcp, &read_fds);
	FD_SET(socket_udp, &read_fds);
	FD_SET(0, &read_fds); // input stdin
	fdmax = max(socket_tcp, socket_udp);

	// serverul rulează până când primește mesajul exit
	while (exit == 0) {
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if (ret < 0) {
			perror("[ERROR] Failed to select!");
		}
		for (int i = 0; i <= fdmax; i++) {
			// verifică dacă file descriptorul face parte din tmp_fds
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == socket_tcp) {
					// CLIENT TCP
					// a venit o cerere de conexiune pe socketul
					// pe care serverul o accepta
					new_socket = accept(socket_tcp, (struct sockaddr *) &address_tcp, &size_address);
					if (new_socket < 0) {
						perror("[ERROR] Failed to accept new TCP client!");
					}
					// se dezactivează altoritmul lui Nagle
					int flag = 1;
					setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
					if (flag < 0) {
						perror("[ERROR] Failed to disable Nagle's algorithm!");
					}

					FD_SET(new_socket, &read_fds);
					if (new_socket > fdmax) {
						fdmax = new_socket;
					}

					memset(buffer, 0, BUFLEN);
					n = recv(new_socket, buffer, sizeof(buffer), 0);
					if (n < 0) {
						perror("[ERROR] Failed to receive IP from new TCP client");
					}

					string new_client_id = buffer;
	
					auto find = status_client.find(new_client_id);
					if (find != status_client.end()) {

						// verific dacă clientul s-a reconectat sau dacă este prima dată online
						if (status_client[new_client_id] == 0) {
							// Client reconectat
							cout << "Client <"<< new_client_id<<"> connected again from "
							<< inet_ntoa(address_tcp.sin_addr) <<":"
							<< ntohs(address_tcp.sin_port) <<"." <<'\n';


							// deschid și trimit mesajele pe care le-am primit în timp ce era offline
							string file_name = new_client_id + ".txt";
							int file = open(file_name.data(), O_RDONLY);
							memset(buffer, 0, BUFLEN);

							read(file, buffer, BUFLEN);
							n = send(new_socket, buffer, BUFLEN, 0);
							if (n < 0) {
								perror("[ERROR] Failed to send subscription!");
							}
							memset(buffer, 0, BUFLEN);

							close(file);
							file = open(file_name.data(), O_RDONLY);

							while(read(file, buffer, BUFLEN)) {
								n = send(new_socket, buffer, BUFLEN, 0);
								if (n < 0) {
									perror("[ERROR] Failed to send subscription!");
								}
								memset(buffer, 0, BUFLEN);
							}
							close(file);

							// șterg datele fișierului
							file = open(file_name.data(), O_TRUNC);
							close(file);

							// îl trec ca fiind online
							status_client[new_client_id] = 1;
							// îmi actualizez unordered_map-urile legate de relația id-socket
							find_id.insert({{new_socket, new_client_id}});
							find_socket.insert({{new_client_id, new_socket}});
						} else if (status_client[new_client_id] == 1) {
							// Client deja activ
							// Cod ID deja folosit.
							cout << "Client <"<< new_client_id<<"> already connected" <<"'\n";
							memset(buffer, 0, BUFLEN);
							buffer[0] = 'i'; // id
							// anunț clientul de eroare
							ret = send(new_socket, buffer, BUFLEN, 0);
							if (ret < 0) {
								perror("[ERROR] Failed to send when client already connected!");
							}
							n = recv(new_socket, buffer, sizeof(buffer), 0);
							if (n < 0) {
								perror("[ERROR] Failed to receive when client already connected!");
							}
							// închid sochetul
							close(new_socket);
							FD_CLR(new_socket, &read_fds);
						}
					} else {
						// Client nou
						// Imi creez instanțe noi pentru fiecare unordered_map în parte
						status_client.insert({{new_client_id, 1}});
						memset(buffer, 0, BUFLEN);
						// trimit mesaj de confirmare al ID-ului
						send(new_socket, buffer, BUFLEN, 0);
						cout << "New client <" << new_client_id << "> connected from "
						<< inet_ntoa(address_tcp.sin_addr)
						<< ":" << ntohs(address_tcp.sin_port) << "." <<'\n';

						find_id.insert({{new_socket, new_client_id}});
						find_socket.insert({{new_client_id, new_socket}});
					}
				}
				if (i == socket_udp) {
					// CLIENT UDP
					memset(buffer, 0, BUFLEN);
					// primesc un mesaj de la clientul UDP
					ret = recvfrom(socket_udp, buffer, BUFLEN, 0, (struct sockaddr *) &address_udp, &size_address);
					if (ret == -1) {
						perror("[ERROR] Failed to receive from UDP!");
					}
					// mesajul de la server este de forma IP:PORT client_UDP -topic - tip_date -valoare mesaj
					// îmi preiau datele necesare pentru a creea mesajul ce trebuie trimis la clientul TCP
					char IP[16], topic[51], message[1500];
					unsigned short port;
					int tip_date;
					strcpy(IP, inet_ntoa(address_udp.sin_addr));
					port = ntohs(address_udp.sin_port);
					memcpy(topic, buffer, 50);
					memcpy(&tip_date, buffer + 50, 1);
					memcpy(message, buffer + 51, 1500);
					string new_topic = topic;
	    			topic[50] = '\0';
	
					char mess[BUFLEN];
					memset(mess, 0, BUFLEN);
				
					// în funcție de tipul de date fac tranformările necesare
					if (tip_date == 0) {
						// INT
						int number_sign =  message[0];
						uint32_t number = 0;
						number = ntohl(*(uint32_t*)(message + 1));
						if (number_sign == 0) {
							snprintf(mess, BUFLEN,"%s: %d - %s - INT - %d\n", IP, port, topic, number);
						} else {
							snprintf(mess, BUFLEN,"%s: %d - %s - INT - -%d\n", IP, port, topic, number);
						}
					} else if (tip_date == 1) {
						// SHORT_REAL
						uint16_t number = 0;
						number = ntohs(*(uint16_t*)message);
						double my_number = number / 100.0f;
						snprintf(mess,BUFLEN,"%s: %d - %s - SHORT_REAL - %.2f\n", IP, port, topic, my_number);
					} else if (tip_date == 2) {
						// FLOAT
						int number_sign = message[0];
						uint32_t number = 0;
						unsigned char this_number[4];
						memcpy(this_number, message + 1, 4);

						number = ntohl(*(uint32_t*) this_number);
						double final;
						final = number * pow(10,  -message[5]);

						if (number_sign == 0) {
							snprintf(mess, BUFLEN,"%s: %d - %s - FLOAT - %.*f\n", IP, port, topic, message[5], final);
						} else {
							snprintf(mess, BUFLEN,"%s: %d - %s - FLOAT - -%.*f\n", IP, port, topic, message[5], final);
						}
					} else if (tip_date == 3) {
						// STRING
						snprintf(mess, BUFLEN,"%s: %d - %s - STRING - %s\n", IP, port, topic, message);
					} 
				
					// verific dacă există topicul deja
					auto does_topic_exit = topic_with_clients.find(new_topic);
					if (does_topic_exit != topic_with_clients.end()) {
						vector<string> subscribers = topic_with_clients[new_topic];
						// dacă da, caut prin lista lui de subscriberi
						for (string subscriber : subscribers) {

							if (status_client[subscriber] == 1) {
								// pentru cei activi, trimit mesajul
								n = send(find_socket[subscriber], mess, sizeof(mess), 0);
							} else if (status_client[subscriber] == 0) {
								// iar pentru cei ce au topicul în lista lor de abonamente permanente
								vector<string> this_subsc = permanent[subscriber];
							
								auto this_client = find(this_subsc.begin(), this_subsc.end(), new_topic);

								if (this_client != this_subsc.end()) {
									string file_name = subscriber + ".txt";
									// deschid și scriu mesajul în fisierul cu numele id-ului
									int file = open(file_name.data(), O_CREAT | O_APPEND | O_WRONLY, 0644);
									write(file, mess, BUFLEN);
									close(file);	
								}
							}
						} 
					} else {
						// dacă am un topic nou, îl adaug în unordered_map-ul de topicuri
						vector<string> zero_clients;
						topic_with_clients.insert(make_pair(new_topic, zero_clients));
					}
				}
				if (i == 0) {
					// comanda stdin
					fgets(buffer, BUFLEN, stdin);

					// dacă am exit
					if (strcmp (buffer, "exit\n") == 0) {
						exit = 1;
						for (auto  element : status_client) {
							if (element.second == 1) {
								// inchid toți clienții care sunt conectați
								memset(buffer, 0, BUFLEN);
								buffer[0] = 'i';
								n = send(find_socket[element.first], buffer, BUFLEN, 0);
								if (n < 0) {
									perror("[ERROR] Failed to send error to client!");
								}
							}
						}
					} else {
						printf("Did you want to write 'exit'? Type again!\n");
					}
				}
					
				if (status_client[find_id[i]] == 1) {
					// se primește o comanda de la un client TCP
					// așa că serverul trebuie să le recepționeze
					// clientul este online
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					if (n < 0) {
						perror("[ERROR] Failed to receive from TCP");
					}

					if (n == 0) {
						// se deconecteaza de la server
						close(i);
						string id_subscriber = find_id[i];
						status_client[id_subscriber] = 0;
						find_id.erase(i);
						find_socket.erase(id_subscriber);
						FD_CLR(i, &read_fds);
					} else {
						// altfel 
						vector<string> tcp_request;
						string message = buffer;
						int begin = 0;
						int word_size = 0;
						int more_words = 0;

						// iau mesajul recepționat și îl impart în funcție de cuvinte într-un vector
						int len = strlen(buffer);
						for (int i = 0; i < len; i++) {
							if (message[i] == ' ' || message[i] == '\n') {
								tcp_request.push_back(message.substr(begin, word_size));
								word_size = 0;
								begin = i + 1;
								more_words++;
							} else {
								word_size++;
							}
						}	
						if (more_words == 0) {
							tcp_request.push_back(message.substr(0, word_size - 1));
						}

						// dacă primul element din vector este 'exit'
						if (tcp_request[0] == "exit") {
							close(i);
							// are loc deconectarea ca mai devreme
							cout << "Client <" << find_id[i] << "> disconnected!\n";
							string id_subscriber = find_id[i];
							// actualizeaza statusul clientului
							status_client[id_subscriber] = 0;
							// sterge asocierea socket-id
							find_id.erase(i);
							find_socket.erase(id_subscriber);

							FD_CLR(i, &read_fds);

						}
						// dacă primul element din vector este 'subscribe'
						if (tcp_request[0] == "subscribe") {
							string subscriber_id = find_id[i];
							string sub_topic = tcp_request[1];
							int sf = atoi(tcp_request[2].data());
						
							// verific dacă clientul se afla in lista de subscriberi ai topicului
							auto does_topic_exist = topic_with_clients.find(sub_topic);
							if (does_topic_exist == topic_with_clients.end()) {
								vector<string> topic_subscribers;
								topic_subscribers.push_back(subscriber_id); 
								topic_with_clients.insert({{sub_topic, topic_subscribers}});
							} else {
								vector<string> all_subs_topic = topic_with_clients[sub_topic];

								auto look = find(all_subs_topic.begin(), all_subs_topic.end(), subscriber_id);
								if (look == all_subs_topic.end()) {
									// daca nu, il adaug 
									all_subs_topic.push_back(subscriber_id);

									topic_with_clients[sub_topic] = all_subs_topic;
									// verific ce fel de abonament vrea

									if (sf == 1) {
										// vrea permanent
										vector<string> this_subsc = permanent[subscriber_id];
										this_subsc.push_back(sub_topic);
										permanent[subscriber_id] = this_subsc;
										// am adaugat topicul in lista de topicuri permanente
									}	
								} else {
									// daca l-am gasit, verificam daca vrea sa schimbe abonamentul
									vector<string> this_subsc = permanent[subscriber_id];
									auto this_client = find(this_subsc.begin(), this_subsc.end(), sub_topic);

									if (sf == 0 && this_client != this_subsc.end()) {
										this_subsc.erase(this_client);
									} else if (sf == 1 && this_client == this_subsc.end()) {
										this_subsc.push_back(sub_topic);
									}
									permanent[subscriber_id] = this_subsc;
								}
							}
						}
						// daca primul element din vector este unsubscribe
						if (tcp_request[0] == "unsubscribe") {
							string sub_topic = tcp_request[1];

							string subscriber_id = find_id[i];
							// verific daca topicul de la care vrea sa se dezaboneze exista
							auto does_topic_exist = topic_with_clients.find(sub_topic);
							if (does_topic_exist == topic_with_clients.end()) {
								// daca nu, trimit eroare
								memset(buffer, 0, BUFLEN);
								buffer[0] = 't'; //topic
								n = send(i, buffer, BUFLEN, 0);
								if (n < 0) {
									perror("[ERROR] Failed to send error to client!");
								}

							} else {
								// daca exista,
								vector<string> all_subs_topic = topic_with_clients[sub_topic];
								auto look = find(all_subs_topic.begin(), all_subs_topic.end(), subscriber_id);
								// caut daca clientul apartine listei de subscriberi a topicului
								if (look == all_subs_topic.end()) {
									// trimit eroare in caz negativ
									memset(buffer, 0, BUFLEN);
									buffer[0] = 'u'; // unsubscribe
									n = send(i, buffer, BUFLEN, 0);
									if (n < 0) {
										perror("[ERROR] Failed to send error to client");
									}
								} else {
									// elimin clientul din lista de abonati in cazul pozitiv
									if (look != all_subs_topic.end()) {
										all_subs_topic.erase(look);
										topic_with_clients[sub_topic] = all_subs_topic;
									}
									vector<string> this_subsc = permanent[subscriber_id];
									auto this_client = find(this_subsc.begin(), this_subsc.end(), sub_topic);

									if (this_client != this_subsc.end()) {
										this_subsc.erase(this_client);
										permanent[subscriber_id] = this_subsc;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	close(socket_tcp);
	close(socket_udp);
	return 0;
}