#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>

#include "mp2.h"
#include "util.h"
#include "map.h"

#define kMaxMessageSize 1000

/**
 * Some debug related functions (and related variables)
 */
#ifdef DEBUG
FILE *gLogFile;
char gLogName[20];

#define dbg(...) fprintf(gLogFile, __VA_ARGS__)
#define dbg_init() do {                            \
		snprintf(gLogName, 19, "l_%d.txt", my_id); \
		gLogFile = fopen(gLogName, "a+");          \
	} while(0)
#define dbg_message(msg) do {                                                  \
	fprintf(gLogFile, "Message {\n\tType: %d\n\tSource: %u:%u"                 \
	        "\n\tReturn: %u:%u\n\tDestination: %u\n\tContent: %s"              \
	        "\n\tNext: %p\n}\n", msg->type, msg->source_node.id,               \
	        msg->source_node.port, msg->return_node.id, msg->return_node.port, \
	        msg->destination, msg->content, msg->next);                        \
	} while (0)

#else

#define dbg(...)
#define dbg_init()
#define dbg_message(msg)

#endif

int sock;

node_id_t my_id;
port_t my_port;

int m_value;
size_t gMaxNodeCount; // 2^m_value

char joined_flag;
char adding_node_flag;
char has_no_peers = 1;

struct mp2_message *message_buffer;

struct mp2_node next_node;
struct mp2_node prev_node;

struct mp2_node *finger_table;

void recv_handler();
void message_recieve(const char *buf, port_t source_port);
void message(node_id_t destination, int type, char *content, port_t return_port);
void forward_message(const message_t *message);
unsigned int finger_table_index(node_id_t id);

void send_node_lookup(node_id_t lookup_id);
void send_invalidate_finger(node_id_t invalidate_target,
                            node_id_t invalidate_destination,
                            node_t *message_destination);

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

int udp_send(node_t *node, const char *message)
{
	struct sockaddr_in destaddr;

	memset(&destaddr, 0, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	destaddr.sin_port = htons(node->port);

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

/**
 * Returns non zero if this node is the destination
 */
int is_destination(node_id_t dest)
{
	return has_no_peers || (dest < 0) || (dest <= my_id && dest > prev_node.id);
}

/**
 * Returns non zero if the message has taken an invalid path
 */
int invalid_message_path(const message_t *message)
{
	// XXX Unimplemented currently
	return 0;
}

/**
 * Return the nuber of id's between my_id and id in the direction of message flow
 */
size_t path_distance_to_id(node_id_t id)
{
	return ((id + gMaxNodeCount) - my_id) % gMaxNodeCount;
}

/**
 * Find the index in the finger table that corresponds to the highest id that is
 * less or equal to target_id.
 */
unsigned int finger_table_index(node_id_t target_id)
{
	return log_2(path_distance_to_id(target_id));
}

/**
 * Invalidate finger table entries according to a recieved message
 */
void handle_invalidate_finger(message_t *message)
{
	invalidate_finger_content_t info;
	char *buf = message->content;
	info.target = atoi(buf);

	for (; *buf && *buf == ' '; ++buf); // Skip leading spaces (invalid)
	for (; *buf && *buf != ' '; ++buf); // Skip to first whitespace
	for (; *buf && *buf == ' '; ++buf); // Skip whitespace (should just be one)

	info.destination = atoi(buf);

	// Iterate up through the finger table to invalidate all instances of
	// info.target up to and including the one that holds info.destination
	// by marking them as invalid and issuing a lookup for the valid target
	unsigned int table_index = finger_table_index(info.destination);
	for (size_t i = 0; i <= table_index; ++i) {
		if (finger_table[i].id == info.target && !finger_table[i].invalid) {
			finger_table[i].invalid = 1;
			send_node_lookup(((1 << table_index) + my_id) % gMaxNodeCount);
		}
	}
}

/**
 * Handle a node_lookup request by replying with the destination id (so that the
 * requesting node knows what message we are replying to)
 */
void handle_node_lookup(message_t *msg)
{
	char buf[kMaxMessageSize];
	snprintf(buf, kMaxMessageSize, "%d", msg->destination);
	message(msg->source_node.id, node_lookup_ack, buf, my_port);
}

/**
 * Handle a node_lookup response by checking our finger table to update the entry
 */
void handle_node_lookup_ack(message_t *message)
{
	node_id_t dest = atoi(message->content);
	unsigned int table_index = finger_table_index(dest);
	if (finger_table[table_index].invalid) {
		finger_table[table_index].id = message->source_node.id;
	}
}

void recv_handler()
{
	struct sockaddr_in fromaddr;
	socklen_t len;
	int nbytes;
	enum rpc_opcode cmd;
	char buf[kMaxMessageSize];

	while (1)
	{
		len = sizeof(fromaddr);
		nbytes = recvfrom(sock, buf, kMaxMessageSize, 0,
		                  (struct sockaddr *)&fromaddr, &len);

		if (nbytes < 0)
		{
			// May want to tell the ring to quit at this point if we have errs like this.
			fprintf(stderr,"Error in recv from socket\n");
			continue;
		}

		dbg("%d: %s\n", my_id, buf);

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
				message_recieve(buf, 0); // XXX I don't know how to get the port
				break;
		}

	}

}

/**
 * Parse a character buffer into a message object, returns the new object which
 * must be free'd by the caller.
 */
message_t *unmarshal_message(const char *buf)
{
	message_t *message = malloc(sizeof(message_t));
	message->type = atoi(buf);

	for (; *buf && *buf == ' '; ++buf); // Skip leading spaces (invalid)
	for (; *buf && *buf != ' '; ++buf); // Skip to first whitespace
	for (; *buf && *buf == ' '; ++buf); // Skip whitespace (should just be one)

	message->source_node.id = atoi(buf);

	for (; *buf && *buf != ' '; ++buf); // Skip to next white space
	for (; *buf && *buf == ' '; ++buf); // Skip over white space (should just be one)

	message->source_node.port = atoi(buf);

	for (; *buf && *buf != ' '; ++buf); // Skip to next white space
	for (; *buf && *buf == ' '; ++buf); // Skip over white space (should just be one)

	message->return_node.id = atoi(buf);

	for (; *buf && *buf != ' '; ++buf); // Skip to next white space
	for (; *buf && *buf == ' '; ++buf); // Skip over white space (should just be one)

	message->return_node.port = atoi(buf);

	for (; *buf && *buf != ' '; ++buf); // Skip to next white space
	for (; *buf && *buf == ' '; ++buf); // Skip over white space (should just be one)

	message->destination = atoi(buf);

	for (; *buf && *buf != ' '; ++buf); // Skip to next white space
	for (; *buf && *buf == ' '; ++buf); // Skip over white space (should just be one)

	message->content = strdup(buf); // The remainder of the message is "content"

	message->next = NULL;
	message->source_node.invalid = 0;
	message->return_node.invalid = 0;

	return message;
}

/**
 * Encode a message to be sent over the network
 * @return the number of bytes written to buf
 */
int marshal_message(char *buf, const message_t *message)
{
	assert(!message->source_node.invalid);
	assert(!message->return_node.invalid);

	// Handle messages with no content by appending a null terminator
	const char *content = message->content;
	if (!content) {
		content = '\0';
	}

	int characters = sprintf(buf, "%d %u %u %u %u %u %s", message->type,
	                         message->source_node.id, message->source_node.port,
	                         message->return_node.id, message->return_node.port,
	                         message->destination, content);
	if (characters <= 0) {
		fprintf(stderr, "Error marshaling message: %s\n", message->content);
		return 0;
	}

	return characters;
}

/**
 * Free a message
 */
void free_message(message_t *message)
{
	free(message->content);
	free(message);
}

/**
 * Handles recieved node to node messages by either forwarding the message closer
 * to its destination node, or if it is the destination by handling the contents
 * @todo implement finger table correction
 */
void message_recieve(const char *buf, port_t source_port)
{
	message_t *message = unmarshal_message(buf);
	if (message->type <= node_commands || message->type >= last_rpc_opcode) {
		fprintf(stderr, "Recieved invalid message type %u:\n%s\n", message->type, buf);
		goto end;
	}

	if (!is_destination(message->destination)) {
		// Check that the sender's finger table isn't out of date
		if (invalid_message_path(message)) {
			node_t source;
			source.id = -1;
			source.port = source_port;
			source.invalid = 0;
			send_invalidate_finger(my_id, message->destination, &source);
		}

		// send the message on to the next closest node
		forward_message(message);
	} else {
		// We are the intended recipient
		dbg("Recieved:\n");
		dbg_message(message);
		switch (message->type) {
		invalidate_finger:
			handle_invalidate_finger(message);
			break;
		node_lookup:
			handle_node_lookup(message);
			break;
		node_lookup_ack:
			handle_node_lookup_ack(message);
			break;
		default:
			break;
		}
	}

end:
	free_message(message);
}

/**
 * Send a message to destination
 */
void message(node_id_t destination, int type, char *content, port_t return_port)
{
	message_t message;
	message.type = type;
	message.source_node.id = my_id;
	message.source_node.port = my_port;
	message.return_node.id = my_id;
	message.return_node.port = return_port;
	message.content = content;
	message.destination = destination;

	forward_message(&message);
}

/**
 * Send a message invalidating finger entries that would map invalidate_target as
 * a valid intermediate destination for invalidate_destination.
 */
void send_invalidate_finger(node_id_t invalidate_target, node_id_t invalidate_destination,
                            node_t *message_destination)
{
	char content[kMaxMessageSize];
	snprintf(content, kMaxMessageSize, "%d %d", invalidate_target,
	         invalidate_destination);
	message(message_destination->id, invalidate_finger, content, my_port);
}

/**
 * Send a message to find which node "owns" lookup_id
 */
void send_node_lookup(node_id_t lookup_id)
{
	message(lookup_id, node_lookup, NULL, my_port);
}

/**
 * Perform the actual sending of a message to the "closest" node we know of
 */
void forward_message(const message_t *message)
{
	char buf[kMaxMessageSize];

	node_t *dest = finger_table + finger_table_index(message->destination);
	while (dest->invalid) --dest; // Skip invalid entries

	marshal_message(buf, message);

	udp_send(dest, buf);
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

	gMaxNodeCount = 1 << m_value;

	my_id = atoi(argv[2]);
	report_port = atoi(argv[3]);

	if (report_port < 1 || report_port > 65535)
	{
		fprintf(stderr, "Report port is incorrect\n");
		exit(1);
	}

	dbg_init();

	init_socket();

	if (my_id == 0)
		send_port_to_listener(report_port);

	recv_handler();

	return 0;
}
