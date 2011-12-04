#ifndef MP2_H
#define MP2_H

#include <stddef.h>
#include <stdint.h>

#define MP2_NODE_EXEC "mp2_node"

typedef uint32_t port_t;
typedef int32_t node_id_t;

typedef struct mp2_node
{
	node_id_t id;
	port_t port;
	char invalid;
} node_t;

typedef struct mp2_message
{
	int type;
	node_t source_node;
	node_t return_node;
	node_id_t destination;
	char *content;
	struct mp2_message *next;
} message_t;

typedef struct invalidate_finger_content
{
	node_id_t target;
	node_id_t destination;
} invalidate_finger_content_t;

enum rpc_opcode {
	nop = 0,

	l_add_file = 1,
	l_add_file_complete,
	l_del_file,
	l_find_file,
	l_get_table,
	l_quit,

	l_print,

	l_set_node_zero_port, // listener expects "%d %d", cmd, port

	l_last_rpc_opcode,

	node_oob_commands = 32,
	start_add_node,
	single_node_add_resp,
	add_node_ack,
	last_oob_opcode,

	node_messages = 48,
	add_node,
	stitch_node,
	set_next,
	invalidate_finger,
	update_next,
	request_transfer,
	file_transfer,
	file_transfer_ack,
	file_ack,
	join_finished,
	node_lookup,
	node_lookup_ack,
	delete_file,
	find_file,
	file_not_found,
	quit,

	last_message_opcode
};

void message_free(message_t *message);

#endif
