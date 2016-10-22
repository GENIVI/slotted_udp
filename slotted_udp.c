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

s_udp_err_t s_udp_init_channel(s_udp_channel_t* channel,
							   const char* address,
							   in_port_t port,
							   uint32_t slot,
							   uint32_t min_latency,
							   uint32_t max_latency,
							   uint32_t min_frequency,
							   uint32_t max_frequency,
							   uint8_t is_sender)
{
	if (!channel || !address) {
		fprintf(stderr, "s_udp_init_channel(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}
	
	// Setup address
	memset(&channel->address, 0 , sizeof(channel->address));

	if (!inet_aton(address, &channel->address.sin_addr)) {
		fprintf(stderr, "s_udp_init_channel(): inet_aton(%s): Illegal address\n",
				address);
		return S_UDP_ILLEGAL_ADDRESS;
	}

	channel->address.sin_family = AF_INET;
	channel->address.sin_port = htons(port);

	// Setup other fields
	channel->slot = slot;
	channel->min_latency = min_latency;
	channel->max_latency = max_latency;

	channel->min_frequency = min_frequency;
	channel->max_frequency = max_frequency;

	channel->is_sender = is_sender;
	channel->socket_des = -1;
	channel->slot_count = 0;

	return S_UDP_OK;
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

s_udp_err_t s_udp_send_packet(s_udp_channel_t* channel, const uint8_t* data, uint32_t length)
{
	if (!channel || !data) {
		fprintf(stderr, "s_udp_send_packet(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}

	if (!channel->is_sender) {
		fprintf(stderr, "s_udp_send_packet(): Illegal argument\n");
		return S_UDP_NOT_SENDER;
	}

	if (sendto(channel->socket_des, data, length, 0,
			   (struct sockaddr *) &channel->address, sizeof(channel->address)) < 0) {
		perror("s_udp_send_packet(): sendto()");
		return S_UDP_NETWORK_ERROR;
	}

	return S_UDP_OK;
}

s_udp_err_t s_udp_read_data(s_udp_channel_t* channel,
							uint8_t* data,
							uint32_t max_length,
							ssize_t* length,
							int32_t* skew)
{
	socklen_t addrlen = sizeof(channel->address);

	if (!length || !skew || !data || !channel) {
		fprintf(stderr, "s_udp_read_data(): Illegal argument\n");
		return S_UDP_ILLEGAL_ARGUMENT;
	}
		
	if ((*length = recvfrom(channel->socket_des,
						   data,
						   max_length,
						   0, 
						   (struct sockaddr *) &channel->address,
							&addrlen)) < 0) {
		perror("s_udp_read_data(): recvfrom()");
		return S_UDP_NETWORK_ERROR;
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
		"network error"             // S_UDP_NETWORK_ERROR
	};

	return err_string[code];
}
