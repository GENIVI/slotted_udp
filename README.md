**(C) 2016 Jaguar Land Rover - All rights reserved.**<br>

All documents in this repository are licensed under the Creative
Commons Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.
All code in this repository is licensed under Mozilla Public License
v2 (MPLv2). Click [here](https://www.mozilla.org/en-US/MPL/2.0/) for
details.

# SLOTTED UDP
This repository contains exploratory code to examine the possibilities
of streaming time sensitive data, such as media, over a LAN using
UDP/IP multicast.

If successful, slotted udp can be used as a foundation to implement a
lightweight, hardware independent replacement for OpenAVB.

Currently the code is just an API skeleton that wraps stock
multicast. As we move forward we will implement clocking, skew
detection and additional features to flesh out the implementation
behind the API.

# QUICK START

	make

	[Terminal 1 (Receiver)]
	./slotted_udp_test -r

	[Terminal 2 (Receiver)]
	./slotted_udp_test -r

	[Terminal 3 (Sender)]
	./slotted_udp_test -s
	Hello world.

## Usage
	./slotted_udp_test -s [file_name] | -r [file_name]  [-S slot]
	-S slot          Attach to the given slot (1-65). Default 1

	-s [file_name]   Send file_name over the given slot.
                     Stream from stdin if file_name is not specified.

	-r [file_name]   Receive data from sender and write to file_name
                     Stream to stdout if no file_name is specified

# TODO
* Command line arguments for port and address
* Command line argument for slot count
* Command line argument for max packet size
* Sender to transmit clock
* Reader to read clock to measure latency
* TDMA slotting for sender
* Reader to only process slot specced by s_udp_init_channel()
* Report latency and frequency skew in s_udp_read_data()







