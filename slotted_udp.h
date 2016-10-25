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
#include <time.h>


typedef struct _s_udp_channel_t {
	struct sockaddr_in address; // Multicast address group.
	uint32_t slot;    // Slot to use inside address:port
	uint32_t slot_count;    // Number of slots maintained by this channel
	int32_t socket_des;  // File descriptor
	uint32_t max_latency;         // Max allowed latency, in microseconds
	uint32_t min_latency;         // Min latency. Sleep prior to delivery if packet is received prior to this
	uint32_t min_frequency;       // Minimum allowed packed transmisison frequency, in hz
	uint32_t max_frequency;       // Maximum allowed packet transmission frequency, in hz

	uint64_t transaction_id;      // Current transaction ID for sender.
	                              // Last received transaction ID for receiver.

	uint8_t is_sender;            // Is this context single channel sender, or a receiver channel?
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
	S_UDP_NOT_CONNECTED = 8,
	S_UDP_BUFFER_TOO_SMALL = 9,
	S_UDP_MALFORMED_PACKET = 10,
	S_UDP_SLOT_MISMATCH = 11
} s_udp_err_t;



// Convert a struct filled out by clock_gettime(CLOCK_MONOTONIC,
// struct timespec*) to microseconds.
#define timespec2usec(tp) (((uint64_t) tp.tv_sec) * 1000000LL + \
						   ((uint64_t) tp.tv_nsec) / 1000LL)


// Return microseconds since arbitrary start point.
extern uint64_t s_udp_get_master_clock(s_udp_channel_t* channel);

extern const char* s_udp_error_string(s_udp_err_t code);

extern s_udp_err_t s_udp_init_send_channel(s_udp_channel_t* channel,
										   const char* address,
										   in_port_t port,
										   uint32_t slot,
										   uint32_t slot_count,
										   uint32_t min_latency,
										   uint32_t max_latency,
										   uint32_t min_frequency,
										   uint32_t max_frequency);
								 
extern s_udp_err_t s_udp_init_receive_channel(s_udp_channel_t* channel,
											  const char* address,
											  in_port_t port,
											  uint32_t slot);
								 
extern s_udp_err_t s_udp_attach_channel(s_udp_channel_t* channel);

extern s_udp_err_t s_udp_get_socket_descriptor(s_udp_channel_t* channel,
											   int32_t* result);

extern s_udp_err_t s_udp_send_packet(s_udp_channel_t* channel,
									 const uint8_t* data,
									 uint32_t length);

extern s_udp_err_t s_udp_receive_packet(s_udp_channel_t* channel,
										uint8_t* data,
										uint32_t max_length,
										ssize_t* length,
										uint32_t* latency,
										uint8_t* packet_loss_detected);

extern s_udp_err_t s_udp_destroy_channel(s_udp_channel_t* channel);
