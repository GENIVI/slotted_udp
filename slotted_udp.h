/*
   Copyright (C) 2016, Jaguar Land Rover

   This program is licensed under the terms and conditions of the
   Mozilla Public License, version 2.0.  The full text of the 
   Mozilla Public License is at https://www.mozilla.org/MPL/2.0/

 
   Slotted UDP Multicast Header File
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



typedef struct _s_udp_channel_t {
	struct sockaddr_in address; // Multicast address group.
	uint32_t slot;    // Slot to use inside address:port
	uint32_t slot_count;    // Number of slots maintained by this channel
	int32_t socket_des;  // File descriptor
	uint32_t max_latency;         // Max allowed latency, in microseconds
	uint32_t min_latency;         // Min latency. Sleep prior to delivery if packet is received prior to this
	uint32_t min_frequency;       // Minimum allowed packed transmisison frequency, in hz
	uint32_t max_frequency;       // Maximum allowed packet transmission frequency, in hz
	uint8_t is_sender;            // Is this the channel sender
} s_udp_channel_t;


typedef enum _s_udp_err_t {
	S_UDP_OK = 0,
	S_UDP_NOT_SENDER = 1,
	S_UDP_FREQUENCY_VIOLATION = 2,
	S_UDP_LATENCY_VIOLATION = 3,
	S_UDP_ILLEGAL_ADDRESS = 4,
	S_UDP_SUBSCRIPTION_FAILURE = 5,
	S_UDP_ILLEGAL_ARGUMENT = 6,
	S_UDP_NETWORK_ERROR = 7,
	S_UDP_NOT_CONNECTED = 8
} s_udp_err_t;


extern const char* s_udp_error_string(s_udp_err_t code);

extern s_udp_err_t s_udp_init_channel(s_udp_channel_t*,
									  const char* address,
									  in_port_t port,
									  uint32_t slot,
									  uint32_t min_latency,
									  uint32_t max_latency,
									  uint32_t min_frequency,
									  uint32_t max_frequency,
									  uint8_t is_sender);
								 
extern s_udp_err_t s_udp_attach_channel(s_udp_channel_t* channel);

extern s_udp_err_t s_udp_get_socket_descriptor(s_udp_channel_t* channel, int32_t* result);

extern s_udp_err_t s_udp_send_packet(s_udp_channel_t* channel, const uint8_t* data, uint32_t length);

extern s_udp_err_t s_udp_read_data(s_udp_channel_t* channel,
								   uint8_t* data,
								   uint32_t max_length,
								   ssize_t* length,
								   int32_t* skew);

extern s_udp_err_t s_udp_destroy_channel(s_udp_channel_t* channel);
