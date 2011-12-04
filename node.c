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
#include "queue.h"

#define kMaxMessageSize 1000
#define kDirectDestination -1
#define kTableSize 100

/**
 * Some debug related functions (and related variables)
 */
#ifdef DEBUG
FILE *gLogFile;
char gLogName[20];

#define dbg(...) do {                     \
        fprintf(gLogFile, __VA_ARGS__);   \
        fclose(gLogFile);                 \
        gLogFile = fopen(gLogName, "a+"); \
    } while (0)

#define dbg_init() do {                        \
    snprintf(gLogName, 19, "l_%d.txt", my_id); \
    gLogFile = fopen(gLogName, "w+");          \
} while(0)

#define dbg_message(msg) do {                                                      \
    fprintf(gLogFile, "Message {\n\tType: %d\n\tSource: %u:%u:%d"                  \
            "\n\tReturn: %u:%u:%d\n\tDestination: %u\n\tContent: %s"               \
            "\n\tNext: %p\n}\n", msg->type, msg->source_node.id,                   \
            msg->source_node.port, msg->source_node.invalid,                       \
            msg->return_node.id, msg->return_node.port, msg->return_node.invalid,  \
            msg->destination, msg->content, msg->next);                            \
    fclose(gLogFile);                                                              \
    gLogFile = fopen(gLogName, "a+");                                              \
    } while (0)

#define dbg_finger() do {                                                          \
    int debug_iterator;                                                            \
    assert(finger_table != NULL);                                                  \
    fprintf(gLogFile, "Next node entry, port: %d, id: %d, invalid: %d\n",          \
            next_node.port, next_node.id, next_node.invalid);                      \
    fprintf(gLogFile, "Prev node entry, port: %d, id: %d, invalid: %d\n",          \
            prev_node.port, prev_node.id, prev_node.invalid);                      \
    for (debug_iterator = 0; debug_iterator < m_value; debug_iterator++) {         \
        fprintf(gLogFile, "Finger table entry %d, port: %d, id: %d, invalid %d\n", \
                debug_iterator, finger_table[debug_iterator].port,                 \
                finger_table[debug_iterator].id,                                   \
                finger_table[debug_iterator].invalid);                             \
    }                                                                              \
    fclose(gLogFile);                                                              \
    gLogFile = fopen(gLogName, "a+");                                              \
    } while (0)

#define dbg_udp(port, msg) do {                                         \
    fprintf(gLogFile, "UDP message from: %d content: %s\n", port, msg); \
    fclose(gLogFile); gLogFile = fopen(gLogName, "a+");                 \
} while (0)


#else

#define dbg(...)
#define dbg_init()
#define dbg_message(msg)
#define dbg_udp(port, msg)
#define dbg_finger()

#endif

int sock;

node_id_t my_id;
port_t my_port;

port_t gListenerPort;

int m_value;
size_t gMaxNodeCount; // 2^m_value

char joined_flag;
char adding_node_flag;
char has_no_peers = 1;

struct mp2_message *message_buffer;

node_t next_node;
node_t prev_node;

struct mp2_node *finger_table;

queue_t *gMessageQueue;
queue_t *gRecycleQueue;

hash_table_t *gMap;

void recv_handler();
void message_recieve(const char *buf, port_t source_port);
void message(node_id_t destination, int type, char *content, port_t return_port);
void message_direct(port_t destination, int type, char *content, port_t return_port);
void forward_message(const message_t *message);
unsigned int finger_table_index(node_id_t id);
size_t path_distance_to_id(node_id_t id);
size_t hash_function(int32_t key);

void send_node_lookup(node_id_t lookup_id);
void send_invalidate_finger(node_id_t invalidate_target, node_id_t invalidate_destination, node_t *message_destination);
void process_queue(void);

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

void init_finger_table()
{
	int i;

	dbg("Initalizing finger table\n");

	for (i = 1; i < m_value; i++)
	{
		dbg("sending node lookup for %d\n", ((1 << i) + my_id) % gMaxNodeCount);
		send_node_lookup(((1 << i) + my_id) % gMaxNodeCount);
	}
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

    sendto(sock, message, strlen(message)+1, 0, (struct sockaddr *) &destaddr,
           sizeof(destaddr));
}

int udp_send(port_t dest, const char *message)
{
    struct sockaddr_in destaddr;

    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destaddr.sin_port = htons(dest);

    sendto(sock, message, strlen(message)+1, 0, (struct sockaddr *) &destaddr,
           sizeof(destaddr));

    return 0;
}

void start_join(port_t node_zero_port)
{
    char buf[100];

    snprintf(buf, 99, "%d %d %d", start_add_node, my_id, my_port);
    udp_send(node_zero_port, buf);
}

void start_node_add(char *buf)
{
    char *msg = strstr(buf, " ");
    msg++;

    if (has_no_peers == 0) {
        node_id_t id = atoi(msg);

        dbg("Adding node: %d\n", id);

        message(id, add_node, msg, gListenerPort);
    } else {
        node_t node;
        char resp[100];
        int opcode;

	adding_node_flag = 1;
        sscanf(buf, "%d %d %d", &opcode, &(node.id), &(node.port));

        node.invalid = 0;

        dbg("First node %d being added\n", node.port);

        snprintf(resp, 99, "%d %d %d", single_node_add_resp, my_id, my_port);

        udp_send(node.port, resp);

        finger_table[0].port = node.port;
        finger_table[0].id = node.id;
        finger_table[0].invalid = 0;


        next_node.port = node.port;
        next_node.id = node.id;
        next_node.invalid = 0;

        prev_node.port = node.port;
        prev_node.id = node.id;
        prev_node.invalid = 0;

        has_no_peers = 0;

        dbg_finger();
    }
}

int handle_request_transfer(message_t *msg)
{
    adding_node_flag = 0;
    return 0;
}

void insert_first_node(char *buf)
{
    node_t node_zero;
    int opcode;

    sscanf(buf, "%d %d %d", &opcode, &(node_zero.id), &(node_zero.port));

    node_zero.invalid = 0;

    finger_table[0].port = node_zero.port;
    finger_table[0].id = node_zero.id;
    finger_table[0].invalid = 0;

    next_node.port = node_zero.port;
    next_node.id = node_zero.id;
    next_node.invalid = 0;

    prev_node.port = node_zero.port;
    prev_node.id = node_zero.id;
    prev_node.invalid = 0;

    has_no_peers = 0;

    message_direct(node_zero.port, request_transfer, "", my_port);
	init_finger_table();

    dbg_finger();
}

int initiate_insert(message_t *message)
{
    node_t new_node;
    char msg[100];


    if (adding_node_flag) return 1;
    adding_node_flag = 1;

    dbg("Inserting node using content: %s\n", message->content);
    sscanf(message->content, "%d %d", &new_node.id, &new_node.port);

    new_node.invalid = 0;

    dbg("Sending stitch request to: %d\n", new_node.port);
    snprintf(msg, 99, "%d %d", prev_node.id, prev_node.port);
    message_direct(new_node.port, stitch_node, msg, my_port);

    dbg("Sending set_next to: %d\n", prev_node.port);
    snprintf(msg, 99, "%d %d", new_node.id, new_node.port);
    message_direct(prev_node.port, set_next, msg, my_port);

    has_no_peers = 0;

    prev_node.port = new_node.port;
    prev_node.id = new_node.id;

    dbg_finger();

    return 0;
}

/**
 * Start the processes of quitting by sending a quit message to the next node,
 * but without actually quitting yet (the message will eventually loop around
 * causing this node to quit last)
 */
void initiate_quit(void)
{
	dbg("Initiating Quit\n");
	message_direct(next_node.port, quit, NULL, my_port);
}

/**
 * Start adding a file
 */
void initiate_add_file(char *buf)
{
	assert(!"Adding files unimplemented!");
}

int handle_set_next(message_t *msg)
{

    sscanf(msg->content, "%d %d", &(next_node.id),
	    &(next_node.port));

    next_node.invalid = 0;
    finger_table[0].id = next_node.id;
    finger_table[0].port = next_node.port;
    finger_table[0].invalid = 0;

    dbg_finger();
	return 0;
}

int handle_stitch_node_message(message_t *msg)
{
	next_node.id = msg->source_node.id;
	next_node.port = msg->source_node.port;
    sscanf(msg->content, "%d %d", &(prev_node.id), &(prev_node.port));
    next_node.invalid = 0;
    prev_node.invalid = 0;

    finger_table[0].id = next_node.id;
    finger_table[0].port = next_node.port;
    finger_table[0].invalid = 0;

    has_no_peers = 0;
    dbg_finger();
    message_direct(msg->return_node.port, request_transfer, "", my_port);
	init_finger_table();
	return 0;
}


/**
 * Returns non zero if this node is the destination
 */
int is_destination(node_id_t dest)
{
    return has_no_peers || (dest == kDirectDestination)
	    || (((my_id == 0) || (dest <= my_id)) && dest > prev_node.id);
}

/**
 * Returns non zero if the message has taken an invalid path
 */
int invalid_message_path(const message_t *message)
{
	return path_distance_to_id(message->destination) >
		path_distance_to_id(message->source_node.id);
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
int handle_invalidate_finger(message_t *message)
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

    return 0;
}

/**
 * Handle a node_lookup request by replying with the destination id (so that the
 * requesting node knows what message we are replying to)
 */
int handle_node_lookup(message_t *msg)
{
    char buf[kMaxMessageSize];
    snprintf(buf, kMaxMessageSize, "%d", msg->destination);
    message(msg->source_node.id, node_lookup_ack, buf, my_port);

    return 0;
}

/**
 * Handle a node_lookup response by checking our finger table to update the entry
 */
int handle_node_lookup_ack(message_t *message)
{
    node_id_t dest = atoi(message->content);
    unsigned int table_index = finger_table_index(dest);
	dbg("Adjusting finger table entry %d to read %d:%d\n", table_index, message->source_node.id, message->source_node.port);
    if (finger_table[table_index].invalid) {
        finger_table[table_index].id = message->source_node.id;
	finger_table[table_index].port = message->source_node.port;
	finger_table[table_index].invalid = 0;
    }

	dbg_finger();

    return 0;
}

/**
 * Handle a quit message by forwarding it to any node currently being added and
 * to the next node.
 */
int handle_quit(void)
{
	dbg("Recieved quit\n");
	if (adding_node_flag) {
		message_direct(prev_node.port, quit, NULL, my_port);
	}
	message_direct(next_node.port, quit, NULL, my_port);

	exit(0);

	return 0;
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

        dbg_udp(ntohs(fromaddr.sin_port), buf);

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
	            initiate_quit();
                break;

            case start_add_node:
                start_node_add(buf);
                break;

            case single_node_add_resp:
                insert_first_node(buf);
                break;

            default:
                message_recieve(buf, ntohs(fromaddr.sin_port));
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
	int n = 0;
	message_t *message = calloc(sizeof(message_t), 1);
	int ret = sscanf(buf, "%d %u %u %u %u %u %n", &message->type,
	                 &message->source_node.id, &message->source_node.port,
	                 &message->return_node.id, &message->return_node.port,
	                 &message->destination, &n);
	if (ret != 6) {
		fprintf(stderr, "Error unmarshaling message: %s\n"
		        "\tExpected 6 fields, recieved %d\n", buf, ret);
		message_free(message);
		return NULL;
	}

	assert(n > 0);

	message->content = strdup(buf + n);
    message->next = NULL;
    message->source_node.invalid = 0;
    message->return_node.invalid = 0;

    dbg("unmarshal message\n");
    dbg_message(message);

    return message;
}

/**
 * Encode a message to be sent over the network
 * @return the number of bytes written to buf
 */
int marshal_message(char *buf, const message_t *message)
{
    dbg("marshal message\n");
    dbg_message(message);

    assert(!message->source_node.invalid);
    assert(!message->return_node.invalid);

    // Handle messages with no content by using an empty string
    const char *content = message->content;
    if (!content) {
        content = "";
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
void message_free(message_t *message)
{
    free(message->content);
    free(message);
}

/**
 * Handles recieved node to node messages by either forwarding the message closer
 * to its destination node, or if it is the destination by handling the contents
 */
void message_recieve(const char *buf, port_t source_port)
{
    message_t *message = unmarshal_message(buf);
    if (message->type <= node_messages || message->type >= last_message_opcode) {
        fprintf(stderr, "Recieved invalid message type %u:\n%s\n", message->type, buf);
        message_free(message);
        return;
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
        message_free(message);
    } else {
        // We are the intended recipient, insert it onto our queue
        dbg("Queued:\n");
        dbg_message(message);
	    enqueue(gMessageQueue, message);

	    process_queue();
    }
}

/**
 * Send a message to destination
 */
void message(node_id_t destination, int type, char *content, port_t return_port)
{
    message_t message;
    message.type                = type;
    message.source_node.id      = my_id;
    message.source_node.port    = my_port;
    message.source_node.invalid = 0;
    message.return_node.id      = my_id;
    message.return_node.port    = return_port;
    message.return_node.invalid = 0;
    message.content             = content;
    message.destination         = destination;
    message.next                = NULL;

    forward_message(&message);
}

/**
 * Special case to always message a specific node (bypasses the finger table)
 */
void message_direct(port_t dest, int type, char *content, port_t return_port)
{
	char buf[kMaxMessageSize];
	message_t message;
    message.type                = type;
    message.source_node.id      = my_id;
    message.source_node.port    = my_port;
    message.source_node.invalid = 0;
    message.return_node.id      = my_id;
    message.return_node.port    = return_port;
    message.return_node.invalid = 0;
    message.content             = content;
    message.destination         = kDirectDestination;
    message.next                = NULL;

    marshal_message(buf, &message);
    udp_send(dest, buf);
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
	dbg("forwarding message to %d\n",  dest->id);

    marshal_message(buf, message);

    udp_send(dest->port, buf);
}

/**
 * Process all the currently queued messages
 */
void process_queue(void)
{
	assert(queue_empty(gRecycleQueue));

	while (!queue_empty(gMessageQueue)) {
		int no_free = 0;
		message_t *message = dequeue(gMessageQueue);
		switch (message->type) {
		case invalidate_finger:
			no_free = handle_invalidate_finger(message);
			break;
		case node_lookup:
			no_free = handle_node_lookup(message);
			break;
		case node_lookup_ack:
			no_free = handle_node_lookup_ack(message);
			break;
		case add_node:
			no_free = initiate_insert(message);
			break;
		case stitch_node:
			no_free = handle_stitch_node_message(message);
			break;
		case set_next:
			no_free = handle_set_next(message);
			break;
		case request_transfer:
			no_free = handle_request_transfer(message);
			break;
		case quit:
			no_free = handle_quit();
			break;
		default:
			fprintf(stderr, "Recieved unknown message type: %d\n", message->type);
			break;
		}

		if (no_free) {
			enqueue(gRecycleQueue, message);
		} else {
			message_free(message);
		}
	}

	queue_t *temp = gMessageQueue;
	gMessageQueue = gRecycleQueue;
	gRecycleQueue = temp;
}

size_t hash_function(int32_t key)
{
	return key % kTableSize;
}

int main(int argc, char *argv[])
{
    int report_port;
    int i;

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

    gMessageQueue = queue_init();
    gRecycleQueue = queue_init();

    gMap = new_hash_table(&hash_function, kTableSize);

    init_socket();

    dbg("Initalized, port: %d, id %d\n", my_port, my_id);

    next_node.invalid = 1;
    prev_node.invalid = 1;
    finger_table = calloc(sizeof(node_t), m_value);
    for (i = 0; i < m_value; i++)
        finger_table[i].invalid = 1;

    if (my_id == 0)
    {
        send_port_to_listener(report_port);
        gListenerPort = report_port;
    }
    else
        start_join(report_port);

    recv_handler();

    return 0;
}
