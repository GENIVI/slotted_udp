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
#include <sys/epoll.h>

#define CHANNEL_DEFAULT_ADDRESS "224.0.0.123"
#define CHANNEL_DEFAULT_PORT 49234

void usage(const char* name)
{
	fprintf(stderr, "Usage: %s -s [file_name] | -r [file_name]  [-S slot]\n", name);
	fprintf(stderr, "  -S slot          Attach to the given slot. Default is 1\n\n");
	fprintf(stderr, "  -s [file_name]   Send file_name over the given slot.\n");
	fprintf(stderr, "                   Use '-' to stream from stdin. End with ctrl-d.\n\n");
	fprintf(stderr, "  -r [file_name]   Receive data from sender and write to file_name\n");
	fprintf(stderr, "                   Use '-' to stream to stdout.\n\n");

	fprintf(stderr, "FIXME: Command line arguments for port and address\n");
	fprintf(stderr, "FIXME: Command line argument for max packet size\n");
	fprintf(stderr, "FIXME: TDMA slotting for sender\n");
}

void send_data(s_udp_channel_t* channel, int input_fd)
{
	uint8_t buffer[1024];
	ssize_t rd_len = 0;
	struct epoll_event ev[2];
	int epoll_des;
	int nfds;
	int32_t send_wait = -1;

	/* Code to set up listening socket, 'listen_sock',
	   (socket(), bind(), listen()) omitted */

	epoll_des = epoll_create1(0);
	if (epoll_des == -1) {
		perror("epoll_create1");
		exit(255);
	}

	ev[0].events = EPOLLIN;
	ev[0].data.fd = channel->socket_des;

	if (epoll_ctl(epoll_des, EPOLL_CTL_ADD, channel->socket_des, &ev[0]) == -1) {
		perror("epoll_ctl: channel desc");
		exit(EXIT_FAILURE);
	}

	ev[1].events = EPOLLIN;
	ev[1].data.fd = input_fd;
	if (epoll_ctl(epoll_des, EPOLL_CTL_ADD, input_fd, &ev[1]) == -1) {
		perror("epoll_ctl: input_fd");
		exit(EXIT_FAILURE);

	}
	
	// We need to read packet data from the channel to process
	// slot 0 pacekts sent by the master.
	while(1) {
		int n;
		ssize_t length = 0;
		uint32_t latency = 0;
		uint8_t packet_loss_detected;
		uint64_t tmp_wait = 0;


		nfds = epoll_wait(epoll_des, ev, 2, send_wait);
		if (nfds == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (send_wait != -1 && nfds == 0) {
			printf("Sending %ld bytes\n", rd_len);
			s_udp_send_packet_now(channel, buffer, rd_len);

			send_wait = -1;
			continue;
		}
		
		for (n = 0; n < nfds; ++n) {
			if (ev[n].data.fd == channel->socket_des) {
				printf("Will read packet\n");
				s_udp_receive_packet(channel,
									 buffer,
									 sizeof(buffer),
									 &length,
									 &latency,
									 &packet_loss_detected);
				continue;
			}

			if (ev[n].data.fd == input_fd) {
				rd_len = read(input_fd, buffer, sizeof(buffer));
				if (rd_len == -1) {
					perror("read");
					exit(255);
				}

				if (rd_len == 0)
					return;

				s_udp_get_sleep_duration(channel, &tmp_wait);
				send_wait = (int32_t) (tmp_wait / 1000) + 1;
				printf("Read %ld bytes, will wait %d msec\n", rd_len, send_wait);
				continue;

			}
			printf("Unknown poll hit: %d\n", ev[n].data.fd);
		}
	}
	return;
}


void recv_data(s_udp_channel_t* channel, int output_fd)
{
	uint8_t buffer[1024];
	ssize_t rd_len = 0;
	uint32_t latency = 0;
	uint8_t packet_loss_detected = 0;

	while(1) {
		s_udp_err_t res = S_UDP_OK;

		res = s_udp_receive_packet(channel,
								   buffer,
								   sizeof(buffer),
								   &rd_len,
								   &latency,
								   &packet_loss_detected);

		// This may have been loopback read, or a
		// master packet that was internally processed.
		if (res == S_UDP_TRY_AGAIN)
			continue;
		
		if (rd_len == 0)
			break;
		
		if (res != S_UDP_OK) {
			fprintf(stderr, "Packet receive failed: %s\n",
					s_udp_error_string(res));
			exit(255);
		}
		if (output_fd != 1) {
			printf("t_id[%.9lu] lat[%.5u] len[%.4ld] p_loss[%c]\n",
				   channel->transaction_id,
				   latency,
				   rd_len,
				   packet_loss_detected?'Y':'N');

			write(output_fd, buffer, rd_len);
		}
		else {
			buffer[rd_len] = 0;
			printf("t_id[%.9lu] lat[%.5u] len[%.4ld] p_loss[%c]: %s%c",
				   channel->transaction_id,
				   latency,
				   rd_len,
				   packet_loss_detected?'Y':'N',
				   buffer,
				   (buffer[rd_len-1]=='\n')?0:'\n');
		}
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
	while ((opt = getopt(argc, argv, "s:r:S:")) != -1) {
		switch (opt) {
		case 'r':
			is_sender = 0;
			strncpy(recv_file, optarg, sizeof(recv_file));
			recv_file[sizeof(recv_file)-1] = 0;
			break;


		case 's':
			is_sender = 1;
			strncpy(send_file, optarg, sizeof(send_file));
			send_file[sizeof(send_file)-1] = 0;
			break;

		case 'S':
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
		

	if (s_udp_init_channel(&channel,
						   is_sender,
						   CHANNEL_DEFAULT_ADDRESS,
						   CHANNEL_DEFAULT_PORT,
						   slot) != S_UDP_OK)
			exit(255);

	if (s_udp_attach_channel(&channel) != S_UDP_OK)
		exit(255);

	if (is_sender) {
		int read_fd = -1;

		if (strcmp(send_file, "-")) {
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

		if (strcmp(recv_file, "-")) {
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
