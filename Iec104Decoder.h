#if !defined(_IEC104DECODER_H__INCLUDED_)
#define _IEC104DECODER_H__INCLUDED_

/*if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000*/


#define IEC104_FLAG 0x68

class Iec104Decoder
{

enum STATE {
	IDLE,
	CHECK_LENGTH,
	COLLECTING_FRAME, 
	FRAME_READY,
	FORMAT_ERROR
	   };
private:
	STATE state;
	int number_of_bytes;
//	int ErrorFlag;
public:
	unsigned char frame[255];
public:
	Iec104Decoder() {
		Clear();
//		ErrorFlag = 0;
	}
	~Iec104Decoder() {}
	void Clear(void);
	BOOL Idle(void) { return(state == IDLE); }
	BOOL NewFrameReady(void) { return(state == FRAME_READY); }
	BOOL Error(void) { return(state == FORMAT_ERROR); }
	void PutNextByte(int byte);
	int NumberOfBytes() { return(number_of_bytes); }	
};

#endif // !defined(_IEC104DECODER_H__INCLUDED_)

