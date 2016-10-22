/*
   Copyright (C) 2016, Jaguar Land Rover

   This program is licensed under the terms and conditions of the
   Mozilla Public License, version 2.0.  The full text of the 
   Mozilla Public License is at https://www.mozilla.org/MPL/2.0/

 
   Slotted UDP Multicast Header File
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
#define SLOT_COUNT 65
void usage(const char* name)
{
	fprintf(stderr, "Usage: %s -s [file_name] | -r [file_name]  [-S slot]\n", name);
	fprintf(stderr, "  -S slot          Attach to the given slot (1-%d). Default 1\n\n",
			SLOT_COUNT);
	fprintf(stderr, "  -s [file_name]   Send file_name over the given slot.\n");
	fprintf(stderr, "                   Stream from stdin if file_name is not specified.\n\n");
	fprintf(stderr, "  -r [file_name]   Receive data from sender and write to file_name\n");
	fprintf(stderr, "                   Stream to stdout if no file_name is specified\n\n");

	fprintf(stderr, "FIXME: Command line arguments for port and address\n");
	fprintf(stderr, "FIXME: Command line argument for slot count\n");
	fprintf(stderr, "FIXME: Command line argument for max packet size\n");
	fprintf(stderr, "FIXME: Sender to transmit clock\n");
	fprintf(stderr, "FIXME: Reader to read clock to measure latency\n");
	fprintf(stderr, "FIXME: TDMA slotting for sender\n");
	fprintf(stderr, "FIXME: Reader to only process slot specced by s_udp_init_channel()\n");
	fprintf(stderr, "FIXME: Report latency and frequency skew in s_udp_read_data()\n");
}

void send_data(s_udp_channel_t* channel, int input_fd)
{
	uint8_t buffer[1024];
	ssize_t rd_len = 0;

	while(1) {
		rd_len = read(input_fd, buffer, sizeof(buffer));
		if (rd_len == -1) {
			perror("read");
			exit(255);
		}
		s_udp_send_packet(channel, buffer, rd_len);
		if (rd_len == 0)
			break;
	}
	return;
}

void recv_data(s_udp_channel_t* channel, int output_fd)
{
	uint8_t buffer[1024];
	ssize_t rd_len = 0;
	int32_t skew = 0;


	while(1) {
		s_udp_err_t res = S_UDP_OK;

		res = s_udp_read_data(channel,
							  buffer,
							  sizeof(buffer),
							  &rd_len,
							  &skew);
										  
		if (rd_len == 0)
			break;
		
		if (res != S_UDP_OK) {
			perror(s_udp_error_string(res));
			exit(255);
		}
		write(output_fd, buffer, rd_len);
	}
	return;
}


int main(int argc, char* argv[])
{
	char is_sender = -1;
	int slot = 1;
	int opt;
	s_udp_channel_t channel;
	char recv_file[256];
	char send_file[256];

	recv_file[0] = 0;
	send_file[0] = 0;
	while ((opt = getopt(argc, argv, "s::r::S::")) != -1) {
		switch (opt) {

		case 's':
			is_sender = 1;
			if (optarg) {
				strncpy(send_file, optarg, sizeof(send_file));
				send_file[sizeof(send_file)-1] = 0;
			}
			break;

		case 'r':
			is_sender = 0;
			if (optarg) {
				strncpy(recv_file, optarg, sizeof(recv_file));
				recv_file[sizeof(recv_file)-1] = 0;
			}
			break;

		case 'S':
			if (optarg) 
				slot = atoi(optarg);
			break;

		default: /* '?' */
			usage(argv[0]);
			exit(255);
		}
	}

	if (is_sender == -1) {
		fprintf(stderr, "Please specify either -s or -r\n\n");
		usage(argv[0]);
		exit(255);
	}
		
	s_udp_init_channel(&channel,
					   CHANNEL_DEFAULT_ADDRESS,
					   CHANNEL_DEFAULT_PORT,
					   slot,  // Channel slot
					   1000,  // Minimum latency, in microseconds  
					   10000, // Maximum latency, in microseconds
					   100,   // Minimum packets per second
					   150,   // Maximum packets per second
					   is_sender);

	s_udp_attach_channel(&channel);

	if (is_sender) {
		int read_fd = -1;

		if (send_file[0]) {
			read_fd = open(send_file, O_RDONLY);
			if (read_fd == -1) {
				perror(send_file);
				exit(255);
			}
		} else
			read_fd = 0; // Stream from stdin.

		send_data(&channel, read_fd);

		close(read_fd);
	} else {

		int write_fd = -1;

		if (recv_file[0]) {

			write_fd = creat(recv_file, 0666);

			if (write_fd == -1) {
				perror(recv_file);
				exit(255);
			}

		} else
			write_fd = 1; // Stream to stdout.

		recv_data(&channel, write_fd);

		close(write_fd);
	}
	s_udp_destroy_channel(&channel);
}
