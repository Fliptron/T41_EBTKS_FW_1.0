/*
Copyright (C) 2016 Arturo Guadalupi. All right reserved.
with modifications by R.Bull 2021

This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
*/

#ifndef _BASE64_H
#define _BASE64_H

#include <Arduino.h>
#if (defined(__AVR__))
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif

const char PROGMEM _Base64AlphabetTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

class Base64Class {

public:

//  setup where to put the output and max size
void beginStreamEncode(char *output)
{
    _triplet = 0;
    _outLen = 0;
    _output = output;
}
//  feed 1 byte at a time
void streamEncode(uint8_t in)
{
    _A3[_triplet++] = in;
    if(_triplet == 3) {
			fromA3ToA4(_A4, _A3);

			for(int i = 0; i < 4; i++) {
				_output[_outLen++] = pgm_read_byte(&_Base64AlphabetTable[_A4[i]]);
			}
			_triplet = 0;
		}
}

// finalise the encoding. return the length of the encoded string

int finalStreamEncode()
{
    if(_triplet) {
		for(int j = _triplet; j < 3; j++) {
			_A3[j] = '\0';
		}

	fromA3ToA4(_A4, _A3);

	for( int j = 0; j < _triplet + 1; j++) {
			_output[_outLen++] = pgm_read_byte(&_Base64AlphabetTable[_A4[j]]);
		}

	while((_triplet++ < 3)) {
			_output[_outLen++] = '=';
		}
	}
	_output[_outLen] = '\0';
	return _outLen;

}

int encode(char *output, char *input, int inputLength) 
{
    beginStreamEncode(output);

    while(inputLength--)
    {
        streamEncode(*(input++));
    }
    return finalStreamEncode();
}

//  deprecated. see above function
//
//	Except it is still referenced in EBTKS_CRT.cpp at line 1148.  Need to resolve with Russell ####
int Xencode(char *output, char *input, int inputLength) {
	int i = 0, j = 0;
	int encodedLength = 0;
	unsigned char A3[3];
	unsigned char A4[4];

	while(inputLength--) {
		A3[i++] = *(input++);
		if(i == 3) {
			fromA3ToA4(A4, A3);

			for(i = 0; i < 4; i++) {
				output[encodedLength++] = pgm_read_byte(&_Base64AlphabetTable[A4[i]]);
			}

			i = 0;
		}
	}

	if(i) {
		for(j = i; j < 3; j++) {
			A3[j] = '\0';
		}

		fromA3ToA4(A4, A3);

		for(j = 0; j < i + 1; j++) {
			output[encodedLength++] = pgm_read_byte(&_Base64AlphabetTable[A4[j]]);
		}

		while((i++ < 3)) {
			output[encodedLength++] = '=';
		}
	}
	output[encodedLength] = '\0';
	return encodedLength;
}

int decode(char * output, char * input, int inputLength) {
	int i = 0, j = 0;
	int decodedLength = 0;
	unsigned char A3[3] = {0,0,0};
	unsigned char A4[4] = {0,0,0,0};

	while (inputLength--) {
		if(*input == '=') {
			break;
		}

		A4[i++] = *(input++);
		if (i == 4) {
			for (i = 0; i <4; i++) {
				A4[i] = lookupTable(A4[i]);
			}

			fromA4ToA3(A3,A4);

			for (i = 0; i < 3; i++) {
				output[decodedLength++] = A3[i];
			}
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++) {
			A4[j] = '\0';
		}

		for (j = 0; j <4; j++) {
			A4[j] = lookupTable(A4[j]);
		}

		fromA4ToA3(A3,A4);

		for (j = 0; j < i - 1; j++) {
			output[decodedLength++] = A3[j];
		}
	}
	output[decodedLength] = '\0';
	return decodedLength;
}

int encodedLength(int plainLength) {
	int n = plainLength;
	return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

int decodedLength(char * input, int inputLength) {
	int i = 0;
	int numEq = 0;
	for(i = inputLength - 1; input[i] == '='; i--) {
		numEq++;
	}

	return ((6 * inputLength) / 8) - numEq;
}

private:

int _triplet;
unsigned char _A3[3];
unsigned char _A4[4];
char *_output;
int _outLen;
//Private utility functions
inline void fromA3ToA4(unsigned char * A4, unsigned char * A3) {
	A4[0] = (A3[0] & 0xfc) >> 2;
	A4[1] = ((A3[0] & 0x03) << 4) + ((A3[1] & 0xf0) >> 4);
	A4[2] = ((A3[1] & 0x0f) << 2) + ((A3[2] & 0xc0) >> 6);
	A4[3] = (A3[2] & 0x3f);
}

inline void fromA4ToA3(unsigned char * A3, unsigned char * A4) {
	A3[0] = (A4[0] << 2) + ((A4[1] & 0x30) >> 4);
	A3[1] = ((A4[1] & 0xf) << 4) + ((A4[2] & 0x3c) >> 2);
	A3[2] = ((A4[2] & 0x3) << 6) + A4[3];
}

inline unsigned char lookupTable(char c) {
	if(c >='A' && c <='Z') return c - 'A';
	if(c >='a' && c <='z') return c - 71;
	if(c >='0' && c <='9') return c + 4;
	if(c == '+') return 62;
	if(c == '/') return 63;
	return -1;
}

};


#endif // _BASE64_H


