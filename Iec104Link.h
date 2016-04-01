// Iec104Link.h: interface for the MwIec104Protocol class.
//
#if !defined(AFX_Iec104Link_H__INCLUDED_)
#define AFX_Iec104Link_H__INCLUDED_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <ostream>

#include "Protocol_Packet.h"
#include "Protocol_Layer.h"
#include "DataLink104.h"
#include "Iec104Decoder.h"
#include "MwIEC104FrameProtocol.h"
#include "MwTriplet.h"

class Iec104Chan;
//class MwIEC104FrameProtocol;

class Iec104Link : public Protocol_Layer 
{
private:
	DataLink104 Link2;
	Iec104Decoder Decoder;
	BOOL TcpIsConnect;
	SOCKET ClientSocket;
	BOOL TcpIsServer;
	MwIEC104FrameProtocol P104;
	Iec104Chan * p104Chan;
	BOOL DataEnableFlag;
	long SendAllDataTimer;
	bool TESTFRactHasBeenSent;

	int  MyProtocolIndex ;

	int  DoDebugPrint;

public:
	void ExportData(WORD Nasdu, TripleType Type, WORD Index, double Value, DWORD Status, LONGLONG Time );
	BOOL DrvDataExchange();
	void RemoteChannelInit( void );
	void Init();
	BOOL Load( const MwString& Config )
	{
	  P104.SetProtocolIndex( MyProtocolIndex );
	  BOOL ret = P104.DrvLoad( Config );
	  DoDebugPrint = P104.DoDebugPrint;
		return TRUE;
	}
	void SetProtocolIndex( int ProtoIdx ) { MyProtocolIndex = ProtoIdx; }
	int  GetProtocolIndex( void ) { return MyProtocolIndex; }
	//BOOL HaveTU( int KP, int Adr ) { if( p104Chan ) return p104Chan->HaveTU(KP Adr) else return FALSE; }

private:
	void Transmit(Protocol_Packet *p_Packet) ;
	void Handle_Receive(unsigned char* buf, int lbuf) ;

	int ReadTcpStream(SOCKET ClientSocket, char* Buffer, int BuffSize );
	int WriteTcpStream(SOCKET ClientSocket, char* Buffer, int BitNumber );
	void TerminateConnection( char* Message );
	int TestSendReady( void );

public:
	Iec104Link(Iec104Chan * p104ChanArg, int IsServerArg, SOCKET ClientSocketArg);
	virtual ~Iec104Link() ;
};

#endif // !defined(AFX_Iec104Link_H__INCLUDED_)
