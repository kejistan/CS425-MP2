#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include "mp2.h"

#define MP2_NODE_EXEC "mp2_node"

int listener_port;
int node_zero_port;

int m_value;

static int sock;

pthread_t net_recv_thread;

void *net_recv_handler(void *eh)
{
	struct sockaddr_in fromaddr;
	socklen_t len;
	int nbytes;
	enum rpc_opcode cmd;
	char buf[1000];
	char *str;

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

		// Handle sanitization and printout here.
		cmd = atoi(buf);

		switch (cmd)
		{
			case l_set_node_zero_port:
				buf[999] = 0;
				str = strstr(buf, " ");	
				if (str != NULL)
				{
					str++;
					node_zero_port = atoi(str);
					if (node_zero_port > 0)
					{
						printf("Setting node zero port to %d\n", node_zero_port);
					} else
						node_zero_port = 0;
				}
				break;

			default:
				break;
		}

	}

	return NULL;

}

void node_zero_init()
{
	int pid, exec_retval;

	char passed_port[20];
	char m_bit_value[5];

	snprintf(passed_port, 19, "%d", listener_port);
	snprintf(m_bit_value, 4, "%d", m_value);

	pid = fork();

	if (pid == 0)
	{
		exec_retval = execl(MP2_NODE_EXEC, MP2_NODE_EXEC, m_bit_value, "0", passed_port, (char *) 0);

	}
	else
		return;

	if (exec_retval != 0)
	{
		fprintf(stderr, "Error in node zero!!! %d\n", exec_retval);
		fprintf(stderr, strerror(errno));
		exit(1);
	}
	else
	{
		exit(0);
	}

}

void listener_net_init()
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

	listener_port = ntohs(servaddr.sin_port);
	fprintf(stderr, "Port number for listener: %d\n", listener_port);

	if (pthread_create(&net_recv_thread, NULL, &net_recv_handler, NULL) != 0) {
		fprintf(stderr,"Error in creating a thread\n");
		exit(1);
	}

}

void udp_send(char *msg)
{
	if (msg == NULL || node_zero_port == 0)
		return;

	struct sockaddr_in destaddr;

	memset(&destaddr, 0, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	destaddr.sin_port = htons(node_zero_port);

	sendto(sock, msg, strlen(msg)+1, 0, (struct sockaddr *) &destaddr, sizeof(destaddr));

	return;
}

void add_new_node(char *nodes)
{
	char buf[100];
	int new_node;

	int max_nodes = 1 << m_value;

	while (nodes != NULL)
	{
		new_node = atoi(nodes);

		if (new_node > 0 && new_node < max_nodes)
		{
			printf("Adding node %d\n", new_node);
			snprintf(buf, 99, "%d %d", l_add_node, new_node);
			udp_send(buf);
		}
		nodes = strstr(nodes, " ");
		if (nodes == NULL)
			break;

		nodes++;
	}
}

void add_new_file(char *args)
{
	char *value, buf[256];

	value = strstr(args, " ");
	if (value != NULL)
	{
		snprintf(buf, 255, "%d %s", l_add_file, args);
		value[0] = 0;
		value++;

		printf("Adding file '%s' with value '%s'", args, value);
		udp_send(buf);
	}
}

void send_quit_cmd()
{
	char buf[20];

	snprintf(buf, 19, "%d", l_quit);
	udp_send(buf);
}

void send_find_file_cmd(char *filename)
{
	char buf[256];

	snprintf(buf, 255, "%d %s", l_find_file, filename);
	udp_send(buf);
}

void send_del_file_cmd(char *filename)
{
    char buf[256];

    snprintf(buf, 255, "%d %s", l_del_file, filename);
    udp_send(buf);
}

void send_get_table_cmd(char *args)
{
	char buf[40];
	int id;

	id = atoi(args);
	snprintf(buf, 39, "%d %d", l_get_table, id);
	udp_send(buf);
}


int main(int argc, char *argv[])
{
	char str[256];
	int len;

	if (argc != 2)
	{
		fprintf(stderr, "Incorrect number of arguments!\n");
		exit(1);
	}

	m_value = atoi(argv[1]);


	if (m_value < 5 || m_value > 10)
	{
		fprintf(stderr, "Incorrect value for m, must be between 5 and 10\n");
		exit(1);
	}

	listener_net_init();

	node_zero_init();

	while (1)
	{

		if(fgets(str, sizeof(str), stdin) == NULL)
		{
			// quit gracefully here... something went bad.  For now we just short of eject the warp core
			exit(1);
		}
		len = strlen(str);

		if (str[len-1] == '\n')
			str[len-1] = 0;

		if (strncmp(str, "ADD_FILE", 8) == 0)
		{
			int i = 8;

			for (; i < len; i++)
			{
				if (str[i] != ' ' && str[i] != 0)
				{
					add_new_file(str+i);
					break;
				}
			}
		}

		else if (strncmp(str, "ADD_NODE", 8) == 0)
		{
			if (str[8] != '\0' && str[9] != '\0')
				add_new_node(str+9);
		}


		else if (strncmp(str, "FIND_FILE", 9) == 0)
		{
			if (str[9] != '\0' && str[10] != '\0')
				send_find_file_cmd(str+10);
		}

		else if (strncmp(str, "DEL_FILE", 8) == 0)
		{
			if (str[8] != '\0' && str[9] != '\0')
				send_del_file_cmd(str+9);
		}

		else if (strncmp(str, "GET_TABLE", 9) == 0)
		{
			if  (str[9] != '\0' && str[10] != '\0')
				send_get_table_cmd(str+10);
		}

		else if (strncmp(str, "QUIT", 4) == 0)
		{
			send_quit_cmd();
			printf("Quitting\n");
			break;
		}

		else if (strncmp(str, "SLEEP", 5) == 0)
		{
			int secs = atoi(str+6);
			sleep(secs);
		}


		else
		{
			printf("? SYNTAX ERROR\n");
		}

	}

	return 0;
}
