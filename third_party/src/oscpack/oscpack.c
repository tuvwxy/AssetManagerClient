/******************************************************************************
 *  oscpack
 *
 *  Copyright (C) 2010-2011 The Regents of the University of California. 
 *  All Rights Reserved.
 *
 *  Sonic Arts Research and Development Group
 *  California Institute for Telocommunications and Information Technology
 *  University of California,
 *  La Jolla, CA 92093
 *
 *  Commercial use of this program without express permission of the
 *  University of California, San Diego, is strictly prohibited. Information
 *  about usage and redistribution, and a disclaimer of all warrenties are
 *  available in the Copyright file provided with this code.
 *
 *  Author: Toshiro Yamada
 *  Contact: toyamada [at] ucsd.edu
 *
 ******************************************************************************/
#include "oscpack.h"

#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

//	Network to Host 

// Following ntohll() and htonll() code snipits were taken from
// http://www.codeproject.com/KB/cpp/endianness.aspx?msg=1661457
#ifndef ntohll
#define ntohll(x) (((int64_t)(ntohl((int32_t)((x << 32) >> 32))) << 32) | \
(uint32_t)ntohl(((int32_t)(x >> 32)))) //By Runner
#endif
//#define ntohf(x) (float)ntohl(x)
static double ntohf(int32_t x) { x = ntohl(x); return *(float*)&x; }
//#define ntohd(x) (double)ntohll(x)
static double ntohd(int64_t x) { return (double)ntohll(x); }


//	Host to Network

#ifndef htonll
#define htonll(x) ntohll(x)
#endif
//#define htonf(x) (int32_t)htonl(*(int32_t*)&x)
static int32_t htonf(float x) { return (int32_t)htonl(*(int32_t*)&x); }
//#define htond(x) (int64_t)htonll(*(int64_t*)&x)
static int64_t htond(double x) { return (int64_t)htonll(*(int64_t*)&x); }


int32_t oscpack(uint8_t* buf, const char* addr, const char* format, ...)
{
	va_list ap;
	int32_t size;
	
	va_start(ap, format);
	size = voscpack(buf, addr, format, ap);
	va_end(ap);
	
	return size;
}

int32_t voscpack(uint8_t* buf, const char* addr, const char* format, va_list ap)
{
	int32_t size = 0, len;
	char* str;
	int32_t bit32;
	int64_t bit64;
	union {
		char c;
		char* cptr;
		int32_t i;
		int64_t h;
		float f;
		double d;
	} bytes;
	
	// Make sure the address starts with '/'
	if (addr && addr[0] != '/') {
		return -1;
	}
	
	// Copy the OSC address
	len = strlen(addr);
	memcpy(buf, addr, len);
	size += len;
	buf += len;
	// Need to pad zero until 32-bit aligned.
	if ((len = (4 - len % 4)) != 0) {
		memset(buf, '\0', len);
		size += len;
		buf += len;
	}
	
	// Length of type tag (+1 for ',')
	len = strlen(format) + 1;
	
	// First create the type tag and error check format
	*(buf++) = ',';
	size++;
	str = (char*)format; // save this so we can reset the pointer position later
	for (; *format != '\0'; ++format) {
		switch (*format) {
			case 'i':	// 32-bit integer
			case 'h':	// 64-bit integer
			case 'f':	// 32-bit float
			case 'd':	// 64-bit float
			case 's':	// string (array of character)
			case 'c':	// ascii character
			case 'T':	// True
			case 'F':	// False
			case 'N':	// Nil
			case 'I':	// Infinitum
				*(buf++) = *format;
				size++;
				break;
			case 'b':	// blob
			case 't':	// timetag
			case 'r':	// 32-bit RGBA color
			case 'm':	// MIDI
			default:	// unknown type!
				return -1;
		}		
	}
	
	if ((len = (4 - len % 4)) != 0) {
		memset(buf, '\0', len);
		size += len;
		buf += len;
	}
	
	for (format = str; *format != '\0'; ++format) {
		switch (*format) {
			case 'i':	// 32-bit integer
				len = sizeof(int32_t);
				bytes.i = va_arg(ap, int32_t);
				bit32 = htonl(bytes.i);
				memcpy(buf, (char*)&bit32, len);
				buf += len;
				size += len;
				break;
				
			case 'h':	// 64-bit integer
				len = sizeof(int64_t);
				bytes.h = va_arg(ap, int64_t);
				bit64 = htonll(bytes.h);
				memcpy(buf, (char*)&bit64, len);
				buf += len;
				size += len;
				break;
				
			case 'f':	// 32-bit float
				len = sizeof(float);
				bytes.f = (float)va_arg(ap, double);
				bit32 = htonf(bytes.f);
				memcpy(buf, (char*)&bit32, len);
				buf += len;
				size += len;
				break;
				
			case 'd':	// 64-bit float
				len = sizeof(double);
				bytes.d = va_arg(ap, double);
				bit64 = htond(bytes.d);
				memcpy(buf, (char*)&bit64, len);
				buf += len;
				size += len;
				break;
				
			case 's':	// string (array of character)
				bytes.cptr = va_arg(ap, char *);
				len = strlen(bytes.cptr);
				memcpy(buf, bytes.cptr, len);
				buf += len;
				size += len;
				
				if ((len = (4 - len % 4)) != 0) {
					memset(buf, '\0', len);
					size += len;
					buf += len;
				}
				break;
			case 'c':	// ascii character
				bytes.c = (char)va_arg(ap, int);
				*(buf++) = bytes.c;
				memset(buf, 0, 3);
				buf += 3;
				size += 4;
				break;
			case 'T':	// True
			case 'F':	// False
			case 'N':	// Nil
			case 'I':	// Infinitum
				break;
				// TODO: implement these formats
			case 'b':	// blob
				break;
			case 't':	// timetag
				break;
			case 'r':	// 32-bit RGBA color
				break;
			case 'm':	// MIDI
				break;
		}
	}
	
	assert(size % 4 == 0);
	return size;
}

int32_t oscsize(const char* addr, const char* format, ...)
{
	va_list ap;
	int32_t size = 0, len;
	char* str;
	
	// Make sure the address starts with '/'
	if (addr && addr[0] != '/') {
		return -1;
	}
	
	// Length of OSC address
	len = strlen(addr);
	size = len + (4 - len % 4);
	
	// +1 for ',' in type tag)
	size++;
	
	va_start(ap, format);
	for (; *format != '\0'; ++format) {
		switch (*format) {
			case 'i':	// 32-bit integer
				(void)va_arg(ap, int32_t);
				size += 5;
				size++;
				break;
			case 'h':	// 64-bit integer
				(void)va_arg(ap, int64_t);
				size += 5;
				size++;
				break;
			case 'f':	// 32-bit float
				(void)va_arg(ap, double);
				size += 5;
				size++;
				break;
			case 'd':	// 64-bit float
				(void)va_arg(ap, double);
				size += 5;
				size++;
				break;
			case 'c':	// ascii character
				(void)va_arg(ap, int);
				size += 5;
				size++;
				break;
			case 's':	// string (array of character)
				str = va_arg(ap, char*);
				len = strlen(str);
				size += len + (4 - len % 4);
				size++;
				break;
			case 'T':	// True
			case 'F':	// False
			case 'N':	// Nil
			case 'I':	// Infinitum
				size++;
				break;
			case 'b':	// blob
			case 't':	// timetag
			case 'r':	// 32-bit RGBA color
			case 'm':	// MIDI
			default:	// unknown type!
				return -1;
		}		
	}
	va_end(ap);
	
	return size;
}
