#ifndef MP2_H
#define MP2_H


struct mp2_message
{
	char *message;
	struct mp2_message *next;
};

struct mp2_node
{
	int id;
	int port;
};







#endif
