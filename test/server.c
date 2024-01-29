#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include <getopt.h>

#define CONCURRENT_MAX          3
#define DEFAULT_SERVER_PORT     9999
#define BUFFER_SIZE             1024

int main(int argc, char* argv[]) {

	int client_fd[CONCURRENT_MAX] = {0};
	int server_sock_fd;
	char send_msg[BUFFER_SIZE];
	char recv_msg[BUFFER_SIZE];
	struct sockaddr_in server_addr;
	int c;
	int port = -1;
	char* ip_addr = NULL;

	if(argc == 1)
		goto no_args;
	static struct option long_options[] = {
			{.name = "port", .has_arg = 1, .val = 'p'},
			{.name = NULL, .has_arg = 0, .val = '\0'}
    };

	c = getopt_long(argc, argv, "p:i:", long_options, NULL);
	if (c == -1) {
		printf("Input params wrong \r\n");
		return -1;
	}
	switch (c)
	{
		case 'p':
			port = strtoul(optarg, NULL, 0);
			break;
		default:
			break;
	}
no_args:
	if(port == -1) {
		printf("Port not set, default to 9999 \r\n");
		port = DEFAULT_SERVER_PORT;
	}

	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;


	server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock_fd < 0) {
		printf("create socket error:%s(errno:%d)\n",strerror(errno),errno);
		return -1;
	}

	if (bind(server_sock_fd, (struct sockaddr *) &server_addr,sizeof(server_addr)) < 0) {
		printf("bind socket error:%s(errno:%d)\n",strerror(errno),errno);
		return -1;
	}

	if (listen(server_sock_fd, 5) < 0) {
		printf("listen socket error:%s(errno:%d)\n",strerror(errno),errno);
		return -1;
	}

	//fd_set
	fd_set server_fd_set; 
	int max_fd = -1;
	struct timeval tv;

	while (1) {
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		FD_ZERO(&server_fd_set); 
		FD_SET(STDIN_FILENO, &server_fd_set);
		if (max_fd < STDIN_FILENO) {
			max_fd = STDIN_FILENO;
		}
		FD_SET(server_sock_fd, &server_fd_set);
		if (max_fd < server_sock_fd) {
			max_fd = server_sock_fd;
		}

		for (int i = 0; i < CONCURRENT_MAX; i++) {
			if (client_fd[i] != 0) {
				FD_SET(client_fd[i], &server_fd_set);
				if (max_fd < client_fd[i]) {
					max_fd = client_fd[i];
				}
			}
		}

		int result = select(max_fd + 1, &server_fd_set, NULL, NULL, &tv);
		if (result < 0) {
			printf("select error:%s(errno:%d)\n",strerror(errno),errno);
			continue;
		} else if (result == 0) {
			printf("select 超时\n");
			continue;
		} else {
			if (FD_ISSET(STDIN_FILENO, &server_fd_set)) {
				bzero(send_msg, BUFFER_SIZE);
				scanf("%s", send_msg);
				if (strcmp(send_msg, "q") == 0) {
					close(server_sock_fd);
					exit(0);
				}
				for (int i = 0; i < CONCURRENT_MAX; i++) {
					if (client_fd[i] != 0) {
						printf("Send to Clinet: ");
						printf("client_fd[%d]=%d\n", i, client_fd[i]);
						send(client_fd[i], send_msg, strlen(send_msg), 0);
					}
				}
			}
			if (FD_ISSET(server_sock_fd, &server_fd_set)) {
				struct sockaddr_in client_address;
				socklen_t address_len;
				int client_sock_fd = accept(server_sock_fd,(struct sockaddr *) &client_address, &address_len);
				printf("new connection client_sock_fd = %d\n", client_sock_fd);
				if (client_sock_fd > 0) {
					int index = -1;
					for (int i = 0; i < CONCURRENT_MAX; i++) {
						if (client_fd[i] == 0) {
							index = i;
							client_fd[i] = client_sock_fd;
							break;
						}
					}
					if (index >= 0) {
						printf("New Clinet[%d] [%s:%d] connect success\n", index,
								inet_ntoa(client_address.sin_addr),
								ntohs(client_address.sin_port));
					} else {
						bzero(send_msg, BUFFER_SIZE);
						strcpy(send_msg, "Server Connection are maximum, connect fail!\n");
						send(client_sock_fd, send_msg, strlen(send_msg), 0);
						printf("Server Connection are maximum, new client[%s:%d] connect fail\n",
								inet_ntoa(client_address.sin_addr),
								ntohs(client_address.sin_port));
					}
				}
			}
			for (int i = 0; i < CONCURRENT_MAX; i++) {
				if (client_fd[i] != 0) {
					if (FD_ISSET(client_fd[i], &server_fd_set)) {
						bzero(recv_msg, BUFFER_SIZE);
						int n = recv(client_fd[i], recv_msg,BUFFER_SIZE, 0);
						if (n > 0) {
							if (n > BUFFER_SIZE) {
								n = BUFFER_SIZE;
							}
							recv_msg[n] = '\0';
							printf("From client [%d] msg:%s\n", i, recv_msg);
						} else if (n < 0) {
							printf("From clinet [%d] recerive msg wrong!\n", i);
						} else {
							FD_CLR(client_fd[i], &server_fd_set);
							client_fd[i] = 0;
							printf("Client [%d] close connection\n", i);
						}
					}
				}
			}
		}
	}

	return 0;
}
