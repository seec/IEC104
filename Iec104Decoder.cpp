#include "StdAfx.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "Iec104Decoder.h"
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

void  Iec104Decoder::Clear(void)
{
	state = IDLE;
	number_of_bytes = 0;
	for (unsigned int i=0; i < sizeof(frame); i++) frame[i]=0;
}

#define CASE(state) switch (state) { case
#define OR break; case
#define END_OF_CASE break; default: ;}
#define BREAK break

void Iec104Decoder::PutNextByte(int byte)
{
								CASE (state)
								IDLE:
	if (byte == IEC104_FLAG) {
		frame[number_of_bytes++] = IEC104_FLAG;
								state = CHECK_LENGTH;
								BREAK;
	} else {
								state = FORMAT_ERROR;
								BREAK;
	}
								OR
								CHECK_LENGTH:
	if (byte < 4 || byte > 253) {
								state = FORMAT_ERROR;
								BREAK;
	} else {
		frame[number_of_bytes++] = byte;
								state = COLLECTING_FRAME;
								BREAK;
	}
								OR
								COLLECTING_FRAME:
	frame[number_of_bytes++] = byte;
	if (number_of_bytes == frame[1] + 2) {
								state = FRAME_READY;
								BREAK;
	}
								OR
								FRAME_READY:
								BREAK;
								OR
								FORMAT_ERROR:
								BREAK;
								END_OF_CASE
}


	
