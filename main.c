/*
 *  main.c
 *
 *  Created on: 18-06-11
 *      Author: rongjiayu
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define MEM_BLOCK_SIZE		(1024*1024)			//1 MByte
//#define SOCKET_PORT			(12345)

uint8_t mem_block[MEM_BLOCK_SIZE];

void printerror (const char* action)
{
	int errnum = errno;
	errno = 0;

	if(errnum > 0){
		printf("An error occured while %s.\nError code: %i\nError description: %s\n",
				action, errnum, strerror(errnum));
	}else if(h_errno > 0){
		printf("An error occured while %s.\nError code: %i\nError description: %s\n",
				action, h_errno, hstrerror(h_errno));

		h_errno = 0;
	}else{
		printf("An error occured while %s.\n There is no error data.\n", action);
	}
}

struct sockaddr_in getipa (const char* hostname, int port)
{
	struct sockaddr_in ipa;
	ipa.sin_family = AF_INET;
	ipa.sin_port = htons(port);

	struct hostent* localhost = gethostbyname(hostname);
	if(!localhost){
		printerror("resolveing localhost");
		return ipa;
	}

	char* addr = localhost->h_addr_list[0];
	memcpy(&ipa.sin_addr.s_addr, addr, sizeof(in_addr_t));

	return ipa;
}

void server (int port)
{
	struct protoent* tcp;
	tcp = getprotobyname("tcp");

	int sfd = socket(PF_INET, SOCK_STREAM, tcp->p_proto);
	if(sfd == -1){
		printerror("createing a tcp socket");
		return;
	}

	struct sockaddr_in isa = getipa("localhost", port);

	if (bind(sfd, (struct sockaddr*)&isa, sizeof isa) < 0) {
		printerror("binding socket to IP address");
		return;
	}

	if(listen(sfd, 1) < 0){
		printerror("setting socket to listen");
		return;
	}

	int cfd;
	do {
		printf("watting connect..\n");
	} while ((cfd = accept(sfd, NULL, NULL)) < 0);

	printf("connected!\n");

	struct timeval timeout = { 5, 0 };	//5s timeout
	setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));
	setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));

	size_t len;

	while (1) {
		len = recv(cfd, mem_block, sizeof(mem_block), 0);
		if(len > 0) {
			printf("send ack..\n");
			send(cfd, "ack", 3, 0);
		}
	}
}

static inline int _get_timediff (struct timeval *a, struct timeval *b) {
	int64_t a_ms = a->tv_sec*1000 + a->tv_usec/1000;
	int64_t b_ms = b->tv_sec*1000 + b->tv_usec/1000;
	return abs((int)(a_ms -  b_ms));
}

void client (const char *p_addr, int port, size_t data_size, int freq)
{
	int delay_ms = 1 * 1000 / freq;

	memset(mem_block, 'A', MEM_BLOCK_SIZE);

	struct protoent* tcp;
	tcp = getprotobyname("tcp");

	int sfd = socket(PF_INET, SOCK_STREAM, tcp->p_proto);
	if(sfd == -1){
		printerror("createing a tcp socket");
		return;
	}

	struct timeval timeout = { 0, 500 };	//500ms timeout
	setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));
	setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));

	struct sockaddr_in isa = getipa(p_addr, port);

	if(connect(sfd, (struct sockaddr*)&isa, sizeof isa) == -1){
		printerror("connecting to server");
		return;
	}

	while (1) {
		uint8_t buff[10];
		struct timeval tick_a, tick_b;
		int past_ms, res, len = 0;

		// clear the recv buff
//		recv(sfd, buff, sizeof(buff), MSG_DONTWAIT);

		gettimeofday(&tick_a, NULL);		// time {

		res = send(sfd, mem_block, data_size, 0);
		if (res >= 0) {
//			len = 1; //假装收到PONG
			len = recv(sfd, buff, sizeof(buff), 0);
		}

		gettimeofday(&tick_b, NULL);		// }
		past_ms = _get_timediff(&tick_a, &tick_b);

		if (res >= 0) {
			if (len > 0) {
				printf("%dms\n", past_ms);
			} else {
				printf("no ack...\n");
			}
		} else {
			printf("failed to send\n");
		}

		if (past_ms < delay_ms) {
			usleep((delay_ms - past_ms)*1000);
		}
	}

//	char buff[255];
//	ssize_t size = recv(sfd, (void*)buff, sizeof buff, MSG_WAITALL);

//	if(size == -1){
//		printerror("recieving data from server");
//	}else{
//		buff[size] = '\0';
//		puts(buff);
//	}

//	shutdown(sfd, SHUT_RDWR);
	return ;

}

void show_usage(const char *p_argv0)
{
	printf("usage: \n");
	printf("  %s c [server] [port] [pack_size] [freq]\n", p_argv0);
	printf("  %s s [port]\n", p_argv0);
}

int main (int argc, char *argv[])
{
	if ((argc == 6) && (0 == strcmp(argv[1], "c"))) {
		const char *host = argv[2];
		int port      = atoi(argv[3]);
		int data_size = atoi(argv[4]);
		int freq      = atoi(argv[5]);
		if (data_size > MEM_BLOCK_SIZE) {
			printf("err: data_size > MEM_BLOCK_SIZE");
			return -1;
		}
		printf("client\n");
		client(host, port, data_size, freq);
	} else if ((argc == 3) && (0 == strcmp(argv[1], "s"))) {
		int port = atoi(argv[2]);

//		printf("not supported yet...\n");
		printf("server\n");
		server(port);
	} else {
		printf("fail\n");
		show_usage(argv[0]);
		return -1;
	}
	return 0;
}


