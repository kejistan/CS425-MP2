#ifndef MP2_H
#define MP2_H

#define MP2_NODE_EXEC "mp2_node"

typedef uint32_t port_t;
typedef uint32_t id_t;

typedef struct mp2_node
{
	id_t id;
	port_t port;
	char invalid;
} node_t;

typedef struct mp2_message
{
	int type;
	node_t source_node;
	node_t return_node;
	id_t destination;
	char *content;
	struct mp2_message *next;
} message_t;

enum rpc_opcode {
	nop = 0,

	l_add_node = 1,
	l_add_file,
	l_del_file,
	l_find_file,
	l_get_table,
	l_quit,

	l_set_node_zero_port, // listener expects "%d %d", cmd, port

	l_last_rpc_opcode,
	node_commands = 32,

	add_node,
	set_next,
	set_prev,
	add_node_ack,
	invalidate_finger,
	update_next,
	request_transfer,
	file_transfer,
	file_ack,
	join_finished,
	node_lookup,
	delete_file,
	find_file,
	file_not_found,
	quit,

	last_rpc_opcode
};







#endif
