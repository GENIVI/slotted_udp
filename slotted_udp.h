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
	uint32_t slot_count;    // Number of slots maintained by this channel. Transmitted by master.
	uint32_t slot_width;    // The duration, in usec, of each slot. Transmitted by master.
	uint8_t is_sender;       // Are we sending or receiving on this channel.
	int32_t socket_des;  // File descriptor
	uint64_t transaction_id;      // Current transaction ID for sender.
	                              // Last received transaction ID for receiver.

	uint64_t master_clock_offset; // Microsecnds that self's clock is ahead of master clock.
	                              // Master clock, sent out by slotted_udp_master program, will
	                              // always have a lower value than the local clock.
} s_udp_channel_t;

typedef enum _s_udp_err_t {
	S_UDP_OK = 0,
	S_UDP_TRY_AGAIN = 1,
	S_UDP_NOT_SENDER = 2,
	S_UDP_FREQUENCY_VIOLATION = 3,
	S_UDP_LATENCY_VIOLATION = 4,
	S_UDP_ILLEGAL_ADDRESS = 5,
	S_UDP_SUBSCRIPTION_FAILURE = 6,
	S_UDP_ILLEGAL_ARGUMENT = 7,
	S_UDP_NETWORK_ERROR = 8,
	S_UDP_NOT_CONNECTED = 9,
	S_UDP_BUFFER_TOO_SMALL = 10,
	S_UDP_MALFORMED_PACKET = 11,
	S_UDP_SLOT_MISMATCH = 12,
	S_UDP_OUT_OF_SYNC = 13,
} s_udp_err_t;



// Return synchronized microseconds since arbitrary start point
// At any given time, this function will return the same
// value for all processes connected to same address/port.
//
// Driven by slotted_udp_master program.
extern uint64_t s_udp_get_master_clock(s_udp_channel_t* channel);

// Return microseconds since arbitrary start point.
extern uint64_t s_udp_get_local_clock(void);

extern const char* s_udp_error_string(s_udp_err_t code);

extern s_udp_err_t s_udp_init_channel(s_udp_channel_t* channel,
									  uint8_t is_sender,
									  const char* address,
									  in_port_t port,
									  uint32_t slot);
								 
extern s_udp_err_t s_udp_init_receive_channel(s_udp_channel_t* channel,
											  const char* address,
											  in_port_t port,
											  uint32_t slot);
								 
extern s_udp_err_t s_udp_attach_channel(s_udp_channel_t* channel);

extern s_udp_err_t s_udp_get_socket_descriptor(s_udp_channel_t* channel,
											   int32_t* result);

extern s_udp_err_t s_udp_get_sleep_duration(s_udp_channel_t* channel,
											uint64_t* result_wait);

extern s_udp_err_t s_udp_wait_and_send_packet(s_udp_channel_t* channel,
											  const uint8_t* data,
											  uint32_t length);

extern s_udp_err_t s_udp_send_packet_now(s_udp_channel_t* channel,
										 const uint8_t* data,
										 uint32_t length);

extern s_udp_err_t s_udp_send_packet_raw(int socket_des,
										 void* address,
										 uint32_t slot,
										 uint64_t transaction_id,
										 uint64_t clock,
										 const uint8_t* data,
										 uint32_t length);

extern s_udp_err_t s_udp_receive_packet(s_udp_channel_t* channel,
										uint8_t* data,
										uint32_t max_length,
										ssize_t* length,
										uint32_t* latency,
										uint8_t* packet_loss_detected);

extern s_udp_err_t s_udp_destroy_channel(s_udp_channel_t* channel);
