#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>

#include "mp2.h"

int sock;

int my_id;
int my_port;

int m_value;

char joined_flag;
char adding_node_flag;

struct mp2_message *message_buffer; 

struct mp2_node next_node;
struct mp2_node prv_node;

struct mp2_node *finger_table;

void init_socket()
{
	struct sockaddr_in servaddr;
	socklen_t len;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		fprintf(stderr,"Error init socket for listening\n");
		exit(1);
	}

	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	servaddr.sin_port=htons(0);
	if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		fprintf(stderr,"error in binding to port\n");
		exit(1);
	}


	len = sizeof(servaddr);
	if (getsockname(sock, (struct sockaddr *) &servaddr, &len) < 0)
	{
		fprintf(stderr,"Error in getting socket name\n");
		exit(1);
	}

	my_port = ntohs(servaddr.sin_port);

}

int udp_send(struct mp2_node node, char *message, struct mp2_node return_node)
{
	//char msg[1000];

	struct sockaddr_in destaddr;

	memset(&destaddr, 0, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	destaddr.sin_port = htons(node.port);

	sendto(sock, message, strlen(message)+1, 0, (struct sockaddr *) &destaddr, sizeof(destaddr));

	return 0;
}


int main(int argc, char *argv[])
{
	int report_port;


	if (argc < 4)
	{
		fprintf(stderr, "%s: Incorrect number of arguments, %d\n", argv[0], argc);
		exit(1);
	}
	
	m_value = atoi(argv[1]);

	if (m_value < 5 || m_value > 10)
	{
		fprintf(stderr, "Incorrect value for m: %d, must be between 5 and 10\n", m_value);
		exit(1);
	}

	my_id = atoi(argv[2]);
	report_port = atoi(argv[3]);

	if (report_port < 1 || report_port > 65535)
	{
		fprintf(stderr, "Report port is incorrect\n");
		exit(1);
	}

	init_socket();

	while (1)
	{
		struct mp2_node node;
		node.port = report_port;

		udp_send(node, "testing\n", node);
		sleep(1);
	}

	return 0;
}
