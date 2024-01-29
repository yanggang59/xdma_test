#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include <getopt.h>

#define BUFFER_SIZE           1024
#define DEFAULT_CLIENT_PORT   9999

int main(int argc, char* argv[]) {

	char recv_msg[BUFFER_SIZE];
	char send_msg[BUFFER_SIZE];
	struct sockaddr_in server_addr;
	int server_sock_fd;
	int port = -1;
	char* ip_addr = NULL;
	if(argc == 1)
		goto no_args;
	while(1) {
		int c;
		static struct option long_options[] = {
				{.name = "port", .has_arg = 1, .val = 'p'},
				{.name = "ip", .has_arg = 1, .val = 'i'},
				{.name = NULL, .has_arg = 0, .val = '\0'}
		};
		c = getopt_long(argc, argv, "p:i:", long_options, NULL);
		if (c == -1) {
			break;
		}
		switch (c)
		{
			case 'p':
				port = strtoul(optarg, NULL, 0);
				printf("[Info] port = %d \r\n", port);
				break;
			case 'i':
				ip_addr = strdup(optarg);
				printf("[Info] IP = %s \r\n", ip_addr);
				break;
			default:
				break;
		}
	}
no_args:
	if(port == -1) {
		printf("[Info] Port not set, default to 9999 \r\n");
		port = DEFAULT_CLIENT_PORT;
	}
	if(!ip_addr) {
		printf("[Info] Server IP not set, default to localhost \r\n");
		ip_addr = "127.0.0.1";
	}

	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(ip_addr);

	if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
		return -1;
	}

	if (connect(server_sock_fd, (struct sockaddr *) &server_addr,sizeof(struct sockaddr_in)) < 0) {
		printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
	}

		fd_set client_fd_set;
		struct timeval tv;

		while (1) {
			tv.tv_sec = 2;
			tv.tv_usec = 0;
			FD_ZERO(&client_fd_set);
			FD_SET(STDIN_FILENO, &client_fd_set);
			FD_SET(server_sock_fd, &client_fd_set);

			select(server_sock_fd + 1, &client_fd_set, NULL, NULL, &tv);
			if (FD_ISSET(STDIN_FILENO, &client_fd_set)) {
				bzero(send_msg, BUFFER_SIZE);
				fgets(send_msg, BUFFER_SIZE, stdin);
				if (send(server_sock_fd, send_msg, BUFFER_SIZE, 0) < 0) {
					printf("send message error: %s(errno:%d)\n",strerror(errno),errno);
				}
			}
			if (FD_ISSET(server_sock_fd, &client_fd_set)) {
				bzero(recv_msg, BUFFER_SIZE);
				int n = recv(server_sock_fd, recv_msg, BUFFER_SIZE, 0);
				if (n > 0) {
					printf("[Info] recv %d byte\n",n);
					if (n > BUFFER_SIZE) {
						n = BUFFER_SIZE;
					}
					recv_msg[n] = '\0';
					printf("[MSG] From Server :%s\n", recv_msg);
				} else if (n < 0) {
					printf("[Error] Receive msg wrongly !\n");
				} else {
					printf("[Info] Server closed!\n");
					close(server_sock_fd);
					exit(0);
				}
			}
		}

	return 0;
}

