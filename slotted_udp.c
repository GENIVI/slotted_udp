/*
   Copyright (C) 2016, Jaguar Land Rover

   This program is licensed under the terms and conditions of the
   Mozilla Public License, version 2.0.  The full text of the 
   Mozilla Public License is at https://www.mozilla.org/MPL/2.0/

 
   Slotted UDP Multicast Header File
*/

#include "slotted_udp.h"
#include <unistd.h>
#include <memory.h>
#include <stdio.h>
#include <endian.h>



// Slot           - uint32_t
// Transaction ID - uint64_t
// Clock          - uint64_t
#define _S_UDP_HEADER_LENGTH \
	(sizeof(uint32_t) +     \
	 sizeof(uint64_t) +     \
	 sizeof(uint64_t))

static s_udp_err_t _init_common(s_udp_channel_t* channel,
								const char* address,
								in_port_t port,
								uint32_t slot)
{
	// Setup address
	memset(&channel->address, 0 , sizeof(channel->address));

	if (!inet_aton(address, &channel->address.sin_addr)) {
		fprintf(stderr, "_init_common(): inet_aton(%s): Illegal address\n",
				address);
		return S_UDP_ILLEGAL_ADDRESS;
	}

	channel->address.sin_family = AF_INET;
	channel->address.sin_port = htons(port);

	// Setup other fields
	channel->slot = slot;

	channel->slot_count = 0;
	channel->slot_width = 0;
	channel->transaction_id = 0;
	channel->master_clock_offset = 0;
	return S_UDP_OK;
}


// Process information received from slotted_udp_master program
// on slot 0.
// See slotted_udp_master.c:send_clock() for encoding details
//
static s_udp_err_t _process_master(s_udp_channel_t* channel,
								   uint64_t transaction_id,
								   uint64_t master_clock)
{
	uint64_t local_clock = s_udp_get_monotonic_clock();
	uint64_t local_master_clock = 0;

	printf("transaction_id[%lX]\n", transaction_id);
	// Extract slot count
	channel->slot_count = be32toh(transaction_id >> 32);

	// Extract slot width (in usec)
	channel->slot_width = be32toh((uint32_t) (transaction_id & 0x00000000FFFFFFFF));
	printf("_process_master(): slot_count[%.4d] slot_width[%.6d]\n",
		   channel->slot_count, channel->slot_width);

	// If this is the first time we receive master clock,
	// update offset to delta between self and master clock.
	// Master clock will always be less than local time.
	//
	if (!channel->master_clock_offset) {
		channel->master_clock_offset = local_clock - master_clock;
		printf("_process_master(): Clock offset set to: %lu\n", channel->master_clock_offset);

	}

	local_master_clock = local_clock - channel->master_clock_offset;


	// Assuming that both master and local clock do not skew
	// in relationship to each other:
	//
	// if local_clock - master_clock < channel->master_clock_offset
	// then this UDP/IP slot 0 packet arrived with less latency
	// than any previous slot 0 packets and can be considered
	// a new "ideal" latency to measure drift against.
	//
	// Remember this in channel->master_clock_offset
	//
	printf("Transmitted master clock [%lu]\n", master_clock);
	printf("Local       master clock [%lu]\n", local_master_clock);
	
	if (master_clock < local_master_clock) {
		printf("_process_master(): Improved latency by [%lu] usec\n",
			   local_master_clock - master_clock);
		channel->master_clock_offset += (local_master_clock - master_clock);
	}

	
	return S_UDP_OK;
}


// destination must be at least _S_UDP_HEADER_LENGTH big
static s_udp_err_t _encode_header(uint8_t* header_buf,
								  uint32_t slot,
								  uint64_t transaction_id,	
								  uint64_t clock)
{
	// Encode header
	*((uint32_t*) header_buf) = htobe32(slot);
	header_buf += sizeof(uint32_t);

	*((uint64_t*) header_buf) = htobe64(transaction_id);
	header_buf += sizeof(uint64_t);

	*((uint64_t*) header_buf) = htobe64(clock);

	return S_UDP_OK;
}


static s_udp_err_t _decode_header(uint8_t*  packet,
								  uint32_t  packet_length,
								  s_udp_channel_t* channel,
								  uint32_t* latency,
								  uint8_t*  packet_loss_detected,
								  uint8_t*  master_detected)
{
	uint64_t transaction_id = 0;
	uint32_t slot = 0;
	uint64_t clock;

	*master_detected = 0;
	

	// Do we have enough data to carry a header?
	if (packet_length < _S_UDP_HEADER_LENGTH)
		return S_UDP_MALFORMED_PACKET;

	// ----
	// Decode slot
	// ----
	slot = be32toh(*((uint32_t*) packet));
	packet += sizeof(uint32_t);
		

		
	// Is this packet the right slot?
	if (slot != channel->slot && slot != 0)
		return S_UDP_SLOT_MISMATCH;

	// Is this a slot 0 clock sync?

	// ----
	// Decode transaction id
	// ----
	transaction_id = be64toh(*((uint64_t*) packet));

	packet += sizeof(uint64_t);


	// ----
	// Decode clock
	// ----
	clock = be64toh(*((uint64_t*) packet));

	// Is this a clock sync?
	// If so decode and update channel
	if (!slot) {
		*master_detected = 1;
		return _process_master(channel, transaction_id, clock);
	}

	
	// Check if we have packet loss.
	// Detection can only be made if we have previously received a packet
	// that we compare with, which is indicated by channel->transaction_id != 0.
	if (channel->transaction_id != 0 &&
		transaction_id != channel->transaction_id + 1)
		*packet_loss_detected = 1;
	else
		*packet_loss_detected = 0;

	// Update last received transaction to detect future packet loss.
	channel->transaction_id = transaction_id;

	// Calculate latency
	*latency = s_udp_get_master_clock(channel) - clock;

	// printf("decode: slot:     %d\n",   slot);
	// printf("decode: tid:      %lu\n", transaction_id);
	// printf("decode: clock:    %lu\n", clock);
	// printf("decode: latency:  %u\n",  *latency);
	return S_UDP_OK;
}


s_udp_err_t s_udp_init_send_channel(s_udp_channel_t* channel,
									const char* address,
									in_port_t port,
									uint32_t slot,
									uint32_t slot_count,
									uint32_t min_latency,
									uint32_t max_latency,
									uint32_t min_frequency,
									uint32_t max_frequency)
{
	if (!channel || !address) {
		fprintf(stderr, "s_udp_init_send_channel(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}
	
	channel->is_sender = 1;
	channel->slot_count = slot_count;
	channel->socket_des = -1;

	channel->min_latency = min_latency;
	channel->max_latency = max_latency;

	channel->min_frequency = min_frequency;
	channel->max_frequency = max_frequency;

	
	return _init_common(channel, address, port, slot);
}

s_udp_err_t s_udp_init_receive_channel(s_udp_channel_t* channel,
									   const char* address,
									   in_port_t port,
									   uint32_t slot)
{
	if (!channel || !address) {
		fprintf(stderr, "s_udp_init_receive_channel(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}
		
	channel->is_sender = 0;
	channel->slot_count = 0;
	channel->socket_des = -1;

	channel->min_latency = 0;
	channel->max_latency = 0;

	channel->min_frequency = 0;
	channel->max_frequency = 0;

	return _init_common(channel, address, port, slot);
}


s_udp_err_t s_udp_attach_channel(s_udp_channel_t* channel)
{
	struct ip_mreq mreq;
	struct sockaddr_in local_address;
	uint32_t flag = 0;

	if (!channel) {
		fprintf(stderr, "s_udp_attach_channel(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}

	channel->socket_des = socket(AF_INET, SOCK_DGRAM, 0);

	if (channel->is_sender) {
		return S_UDP_OK;
	}

	flag = 1;

	if (setsockopt(channel->socket_des,
				   SOL_SOCKET,
				   SO_REUSEADDR,
				   &flag,
				   sizeof(flag)) < 0) {
		perror("s_udp_attach_channel(): setsockopt(REUSEADDR)");
		return S_UDP_SUBSCRIPTION_FAILURE;
	}

	// We are subscribers. Bind local addresss
	memset(&local_address, 0 , sizeof(local_address));
	local_address.sin_family = AF_INET;
	local_address.sin_addr.s_addr = htonl(INADDR_ANY);
	local_address.sin_port = channel->address.sin_port;

	if (bind(channel->socket_des,
			 (struct sockaddr *) &local_address, sizeof(local_address)) < 0) {
		perror("s_udp_attach_channel(): bind()");
		return S_UDP_SUBSCRIPTION_FAILURE;
	}
	
	// Add subscription.
	mreq.imr_multiaddr.s_addr = channel->address.sin_addr.s_addr;         
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);         

	if (setsockopt(channel->socket_des, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		perror("s_udp_attach_channel(): setsockopt(IP_ADD_MEMBERSHIP)");
		return S_UDP_SUBSCRIPTION_FAILURE;
	}
	  
	return S_UDP_OK;
}


s_udp_err_t s_udp_get_socket_descriptor(s_udp_channel_t* channel, int32_t* result)
{
	if (!result) {
		fprintf(stderr, "s_udp_get_socket_descriptor(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}

	if (channel->socket_des == -1) {
		return S_UDP_NOT_CONNECTED;
	}
		
	*result = channel->socket_des;
	return S_UDP_OK;
}


s_udp_err_t s_udp_send_packet(s_udp_channel_t* channel, const uint8_t* payload, uint32_t length)
{
	if (!channel) {
		fprintf(stderr, "s_udp_send_packet(): Illegal argument (channel == 0)\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}

	if (!channel->is_sender) {
		fprintf(stderr, "s_udp_send_packet(): Illegal argument\n");
		return S_UDP_NOT_SENDER;
	}

	channel->transaction_id++;
	return s_udp_send_packet_raw(channel->socket_des,
								 &channel->address,
								 channel->slot,
								 channel->transaction_id,
								 s_udp_get_master_clock(channel),
								 payload,
								 length);
}

s_udp_err_t s_udp_send_packet_raw(int socket_des,
								  void *address,
								  uint32_t slot,
								  uint64_t transaction_id,
								  uint64_t clock,
								  const uint8_t* payload,
								  uint32_t length)
{
	uint8_t header[_S_UDP_HEADER_LENGTH];
	s_udp_err_t enc_res = S_UDP_OK;
	struct msghdr message;
	struct iovec payload_array[2];

	if (!payload) {
		fprintf(stderr, "s_udp_send_packet(): Illegal argument (payload == 0)\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}



	enc_res =_encode_header(header, slot, transaction_id, clock);

	if (enc_res != S_UDP_OK) {
		fprintf(stderr, "s_udp_send_packet(): _encode_header(): %s\n",
				s_udp_error_string(enc_res));
		return enc_res;
	}

	// Setup a message that sends both header and payload
	// in one sendmsg() call.

	// Header
	payload_array[0].iov_base = (void*) header;
	payload_array[0].iov_len = sizeof(header);

	// payload
	payload_array[1].iov_base = (void*) payload;
	payload_array[1].iov_len = length;

	// Message struct that points out the payload_array.
	message.msg_name = (struct sockaddr *) address;
	message.msg_namelen = sizeof(struct sockaddr);
	message.msg_iov = payload_array;
	message.msg_iovlen = sizeof(payload_array) / sizeof(payload_array[0]);
	message.msg_control = 0;
	message.msg_controllen = 0;
	message.msg_flags = 0;

	if (sendmsg(socket_des, &message, 0) < 0) {
		perror("s_udp_send_packet(): sendmsg()");
		return S_UDP_NETWORK_ERROR;
	}

	return S_UDP_OK;
}

s_udp_err_t s_udp_receive_packet(s_udp_channel_t* channel,
								 uint8_t* data,
								 uint32_t max_length,
								 ssize_t* length,
								 uint32_t* latency,
								 uint8_t* packet_loss_detected)
{
	uint8_t header[_S_UDP_HEADER_LENGTH];
	s_udp_err_t dec_res = S_UDP_OK;
	struct msghdr message;
	struct iovec payload_array[2];
	struct sockaddr_in source_address;
	uint8_t master_detected = 0;

	if (!length || !latency || !data ||
		!channel || !packet_loss_detected) {
		fprintf(stderr, "s_udp_read_data(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}

		
	// Setup a receive message context that splits the received
	// packet into a separate header and payload buffer.

	// Header
	payload_array[0].iov_base = (void*) &header;
	payload_array[0].iov_len = sizeof(header);

	// Data
	payload_array[1].iov_base = (void*) data;
	payload_array[1].iov_len = max_length;

	// Message context
	message.msg_name = (struct sockaddr *) &source_address;
	message.msg_namelen = sizeof(source_address);
	message.msg_iov = payload_array;
	message.msg_iovlen = sizeof(payload_array) / sizeof(payload_array[0]);
	message.msg_control = 0;
	message.msg_controllen = 0;
	message.msg_flags = 0;

	// Continue to read until we get something else than
	// a master (slot 0) packet.
		
 	while(1) {
		if ((*length = recvmsg(channel->socket_des, &message, 0)) < 0) {
			perror("s_udp_read_data(): recvfrom()");
			return S_UDP_NETWORK_ERROR;
		}

		// Decode header.
		master_detected = 0;
		dec_res = _decode_header(header,
								 *length,
								 channel,
								 latency,
								 packet_loss_detected,
								 &master_detected);

		if (dec_res != S_UDP_OK) {
			fprintf(stderr, "s_udp_receive_packet(): _decode_header(): %s\n",
					s_udp_error_string(dec_res));
			return dec_res;
		}

		// Subtract header length from received data to
		// get payload length
		*length -= _S_UDP_HEADER_LENGTH;

		// If this was a regular slot packet (slot != 0)
		// break out of read loop.
		if (!master_detected)
			break;

		// This was the master sending out an update, then
		// Loop back and wait or the next package.
	}
	return S_UDP_OK;
}


s_udp_err_t s_udp_destroy_channel(s_udp_channel_t* channel)
{
	if (channel->socket_des != -1)
		close(channel->socket_des);

	channel->socket_des = -1;

	return S_UDP_OK;
}

uint64_t s_udp_get_master_clock(s_udp_channel_t* channel)
{
	if (!channel->master_clock_offset)
		return 0L;
	
	return s_udp_get_monotonic_clock() - channel->master_clock_offset;
}

uint64_t s_udp_get_monotonic_clock()
{
	struct timespec tp;						 

	clock_gettime(CLOCK_MONOTONIC, &tp);
	return timespec2usec(tp);
}


const char* s_udp_error_string(s_udp_err_t code)
{
	static char *err_string[] = {
		"ok",                       // S_UDP_OK
		"not sender",               // S_UDP_NOT_SENDER 
		"frequency violation",      // S_UDP_FREQUENCY_VIOLATION 
		"latency violation",        // S_UDP_LATENCY_VIOLATION
		"illegal address",          // S_UDP_ILLEGAL_ADDRESS
		"subscription failure",     // S_UDP_SUBSCRIPTION_FAILURE
		"illegal argument",         // S_UDP_ILLEGAL_ARGUMENT
		"network error",            // S_UDP_NETWORK_ERROR
		"not connected",            // S_UDP_NOT_CONNECTED
		"buffer too small",         // S_UDP_BUFFER_TOO_SMALL
		"malformed packet",         // S_UDP_MALFORMED_PACKET
		"slot mismatch",            // S_UDP_SLOT_MISMATCH
	};

	return err_string[code];
}
