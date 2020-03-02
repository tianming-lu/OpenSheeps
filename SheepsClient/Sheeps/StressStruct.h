#pragma once

typedef struct {
	char* data;
	int		size;
	int		offset;
}t_socket_buff;

typedef struct {
	int msgLen;
	int cmdNo;
}t_stress_protocol_head;