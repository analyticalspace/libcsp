/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 Gomspace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _CSP_COMPILER_H_
#define _CSP_COMPILER_H_

#ifdef __GNUC__ /* gcc and clang should support these */
#	define CSP_COMPILER_WEAK		__attribute__((weak))
#	define CSP_COMPILER_UNUSED		__attribute__((unused))
#	define CSP_COMPILER_PACKED		__attribute__((packed))
#	define CSP_COMPILER_ALIGNED(N)	__attribute__((aligned(N)))
#	define CSP_COMPILER_CONST		__attribute__((__const__))
#else
#	define CSP_COMPILER_WEAK
#	define CSP_COMPILER_UNUSED
#	define CSP_COMPILER_PACKED
#	define CSP_COMPILER_ALIGNED(N)
#	define CSP_COMPILER_CONST
#endif

#if __GNUC__ >= 7
#	define CSP_COMPILER_FALLTHROUGH __attribute__((fallthrough))
#else
#	define CSP_COMPILER_FALLTHROUGH (void)0
#endif

#endif // _CSP_COMPILER_H_
