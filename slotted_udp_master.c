/*
   Copyright (C) 2016, Jaguar Land Rover

   This program is licensed under the terms and conditions of the
   Mozilla Public License, version 2.0.  The full text of the 
   Mozilla Public License is at https://www.mozilla.org/MPL/2.0/

 
   Slotted UDP Multicast Bus/Clock Master program
*/

#include "slotted_udp.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define CHANNEL_DEFAULT_ADDRESS "224.0.0.123"
#define CHANNEL_DEFAULT_PORT 49234
#define DEFAULT_MASTER_TRANSMIT_INTERVAL 500000 // Clock transmit interval, in usec.
#define DEFAULT_SLOT_COUNT 10 // Clock transmit interval, in usec.
#define DEFAULT_SLOT_WIDTH 10000 // Default slot width, in usec. (10msec)

void usage(const char* name)
{
	fprintf(stderr, "Usage: %s -c slot_count\n", name);
	fprintf(stderr, "  -c slot_count   Number of slots to provision on the given\n");
	fprintf(stderr, "                  multicast address. Default: %d\n\n", DEFAULT_SLOT_COUNT);

	fprintf(stderr, "  -w slot_width   The width of each slot, in usec.\n");
	fprintf(stderr, "                  multicast address. Default: %d\n\n", DEFAULT_SLOT_WIDTH);

	fprintf(stderr, "  -i interval     How often to transmit master clock, in usec.\n");
	fprintf(stderr, "                  Default: %d\n\n", DEFAULT_MASTER_TRANSMIT_INTERVAL);

	fprintf(stderr, "FIXME: Command line arguments for port and address\n");
	fprintf(stderr, "FIXME: Ensure that slot 0 sends are only sent during slot 0 send period\n");
}

void send_clock(s_udp_channel_t* channel,
				uint32_t interval)
{
	uint64_t start_clock = s_udp_get_local_clock();
	uint64_t slot_stats = 0;
	uint64_t sleep_duration = 0;
	s_udp_err_t res = S_UDP_OK;

	channel->master_clock_offset = start_clock;
	while(1) {

		// Transaction ID is replaced by total slot count (upper 32 bits)
		// followed by slot_width in usec (lower 32 bits)
		//
		slot_stats = (uint64_t) ((((uint64_t) htobe32(channel->slot_count)) << 32) |
								 htobe32(channel->slot_width));
		

		// Retrieve number of microseconds to sleep
		res = s_udp_get_sleep_duration(channel,
									   &sleep_duration);

		if (res != S_UDP_OK) {
			fprintf(stderr, "s_udp_send_packet(): s_udp_get_sleep_duration(): %s\n",
					s_udp_error_string(res));
			exit(255);
		}
		   
		usleep(sleep_duration);

		s_udp_send_packet_raw(channel->socket_des,
							  &channel->address,
							  channel->slot, // Always 0
							  slot_stats,  // Transaction
							  s_udp_get_local_clock() - start_clock, // Clock
							  (uint8_t*) "", 0);

		usleep(interval);
	}
	return;
}

int main(int argc, char* argv[])
{
	uint32_t slot_count = DEFAULT_SLOT_COUNT;
	uint32_t slot_width = DEFAULT_SLOT_WIDTH;
	uint32_t transmit_interval = DEFAULT_MASTER_TRANSMIT_INTERVAL;
	int opt;
	s_udp_channel_t channel;


 	while ((opt = getopt(argc, argv, "c:i:w:")) != -1) {
		switch (opt) {
		case 'c':
			slot_count = atoi(optarg);
			break;

		case 'i':
			transmit_interval = atoi(optarg);
			break;

		case 'w':
			slot_width = atoi(optarg);
			break;

		default: /* '?' */
			usage(argv[0]);
			exit(255);
		}
	}

	if (slot_count == 0) {
		fprintf(stderr, "Please specify either -c slot_count\n\n");
		usage(argv[0]);
		exit(255);
	}

	if (s_udp_init_channel(&channel,
						   1,
						   CHANNEL_DEFAULT_ADDRESS,
						   CHANNEL_DEFAULT_PORT,
						   0) != S_UDP_OK)
		exit(255);

	if (s_udp_attach_channel(&channel) != S_UDP_OK)
		exit(255);

	channel.slot = 0;
	channel.slot_count = slot_count;
	channel.slot_width = slot_width;
	send_clock(&channel, transmit_interval);

	s_udp_destroy_channel(&channel);
}
