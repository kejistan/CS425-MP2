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

void recv_handler();

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

void send_port_to_listener(int ret_port)
{
	struct sockaddr_in destaddr;

	memset(&destaddr, 0, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	destaddr.sin_port = htons(ret_port);

	char message[32];

	snprintf(message, 31, "%d %d", l_set_node_zero_port, my_port);

	sendto(sock, message, strlen(message)+1, 0, (struct sockaddr *) &destaddr, sizeof(destaddr));

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

void start_add_new_node(char *buf)
{
	int new_node_id, pid, exec_retval;
	char parent_port[20];
	char m_bit_value[5];
	char new_node_id_s[10];

	buf = strstr(buf, " ");

	if (buf == NULL)
		return;

	new_node_id = atoi(buf);
	
	if (new_node_id < 1)
		return;

	snprintf(parent_port, 19, "%d", my_port);
	snprintf(m_bit_value, 4, "%d", m_value);
	snprintf(new_node_id_s, 9, "%d", new_node_id);

	pid = vfork();

	if (pid == 0)
	{
		exec_retval = execl(MP2_NODE_EXEC,  MP2_NODE_EXEC, m_bit_value, new_node_id_s, parent_port, (char *) 0);
	}

}



void recv_handler()
{
	struct sockaddr_in fromaddr;
	socklen_t len;
	int nbytes;
	enum rpc_opcode cmd;
	char buf[1000];
#ifdef DEBUG
	FILE *f;
	char filename[20];

	snprintf(filename, 19, "l_%d.txt", my_id);
#endif

	while (1)
	{
		len = sizeof(fromaddr);
		nbytes = recvfrom(sock, buf, 1000, 0, (struct sockaddr *)&fromaddr, &len);

		if (nbytes < 0)
		{
			// May want to tell the ring to quit at this point if we have errs like this.
			fprintf(stderr,"Error in recv from socket\n");
			continue;
		}

#ifdef DEBUG
		f = fopen(filename, "a+");

		fprintf(f, "%d: %s\n", my_id, buf);
		fclose(f);
#endif

		// Handle sanitization and printout here.
		cmd = atoi(buf);

		switch (cmd)
		{
			case l_quit:
				exit(1);
				break;

			case l_add_node:
				start_add_new_node(buf);
				break;

			case l_set_node_zero_port:


			default:
				break;
		}

	}

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

	if (my_id == 0)
		send_port_to_listener(report_port);

	recv_handler();

	return 0;
}
