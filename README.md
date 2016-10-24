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

	$ make

	[Terminal 1 (Receiver)]
	$ ./slotted_udp_test -r -

	[Terminal 2 (Receiver)]
	$ ./slotted_udp_test -r -

	[Terminal 3 (Sender)]
	$ ./slotted_udp_test -s -
	Hello world.
	<ctrl-d>

## Usage
	slotted_udp_test -s [file_name] | -r [file_name]  [-S slot]
	  -S slot          Attach to the given slot (1-%d). Default 1
	  -s [file_name]   Send file_name over the given slot.
	                   Use '-' to stream from stdin. End with ctrl-d.
	  -r [file_name]   Receive data from sender and write to file_name.
	                   Use '-' to stream to stdout.

# TODO
* Command line arguments for port and address
* Command line argument for slot count
* Command line argument for max packet size
* TDMA slotting for sender



# UDP/IP PACKET FORMAT

Byte   | Name            | Type        |   Description
-------|-----------------|-------------|------------------
0-3    | slot            | uint32\_t   | Slot that the packet was sent in
4-11   | transaction\_id | uint64\_t   | Incremental transaction ID
12-19  | clock           | uint64\_t   | Monotonic clock of sender, in usec
20-... | data            | opaque      | Data.

The length of data is determined by subtracting 20 from the total length
(header size) of the UDP/IP packet ereceived.





