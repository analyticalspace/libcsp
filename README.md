# The Cubesat Space Protocol

Cubesat Space Protocol (CSP) is a small protocol stack written in C. CSP is designed to ease communication between distributed embedded systems in smaller networks, such as Cubesats. The design follows the IP model and includes a transport protocol, a routing protocol and several MAC-layer interfaces. The core of `libcsp` includes a router, a socket buffer pool, and a connection oriented socket API.

The protocol is based on a 32-bit header containing both transport and network-layer information. The implementation is written in GNU C and is currently ported to run on FreeRTOS or POSIX operating systems such as Linux.

The idea is to give sub-system developers of cubesats the same features of a IP stack, but without adding the huge overhead of the IP header and the complexities of an IP-compliant networking stack. The small footprint and simple implementation allows a small 8-bit system with less than 4 kB of RAM to be fully connected on the network. This allows all subsystems to provide their services on the same network level, without any master node required. Using a service oriented architecture has several advantages compared to the traditional mater/slave topology used on many Cubesats.

 * Standardized network protocol: All subsystems can communicate with each other
 * Service loose coupling: Services maintain a relationship that minimizes dependencies between subsystems
 * Service abstraction: Beyond descriptions in the service contract, services hide logic from the outside world
 * Service re-usability: Logic is divided into services with the intention of promoting reuse.
 * Service autonomy: Services have control over the logic they encapsulate.
 * Service Redundancy: Easily add redundant services to the bus
 * Reduces single point of failure: The complexity is moved from a single master node to several well defines services on the network

The implementation of `libcsp` is written with simplicity in mind, but it's compile time configuration allows it to have some rather advanced features as well:

## Features

 * Thread safe Socket API
 * Router task with Quality of Services (QoS)
 * Connection-oriented operation (RFC 908 and 1151)
 * Connection-less operation (similar to UDP)
 * ICMP/SNMP-like requests such as ping and buffer status.
 * Loopback interface
 * Very Small Footprint 48 kB code and less that 1kB ram required on ARM 
 * Zero-copy buffer and queue system
 * Modular network interface system
 * Modular OS and Compiler intrinsic interface
 * Broadcast traffic
 * Promiscuous mode
 * Encrypted packets with XTEA in CTR mode
 * Truncated HMAC-SHA1 Authentication (RFC 2104)

# ASI Fork Notes

ASI has forked `libcsp` to fix some quality control issues. GomSpace has recently (Jan 2020) been applying very solid patches to `libcsp` that mirrors some of the patches we've created. We're going to do our best to bring in fixes from upstream into our fork.

## Building

We provide a `GNUmakefile` which is Linux specific and will suffice for building the included tests and applications. To adhere to the ASI buildsystem, an `inc.mk` was created to encapsulate building and configuration. Non-ASI building should be able to look at the `inc.mk` file, and modify it to fit your needs. The `inc.mk` appends `CFLAGS` for generic compiler flags (includes, etc), `CCFLAGS` for C specific compiler flags, `CCFLAGS` for C++ specific compiler flags, `LDFLAGS` for Linker flags, `OBJECTS` for the makefile targets, and a few recipes for the libcsp sources. We assume your output will be placed in `build/$(TARGET)/...` and that those directories are created. 

## LGPL Software license

The source code is available under an LGPL 2.1 license. See COPYING for the license text.
