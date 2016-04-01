// Iec104Chan.h: interface for the MwIec104Protocol class.

#if !defined(AFX_IEC104CHAN_H__INCLUDED_)
#define AFX_IEC104CHAN_H__INCLUDED_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <ostream>
#include <list>
using namespace std;

#include "Iec104Link.h"
#include "MwTriplet.h"

void GlobLogPrintf( const char *fmt, ... );

class Iec104Chan {
private:
	Iec104Link * pLink104Client;
	Iec104Link * Selection;
	list<Iec104Link *> List104;

	MwString ChannelConfig;

protected:
	BOOL DrvDataExchange();
	BOOL DrvClear( void );
	BOOL DrvStop( void );

	int  MyProtocolIndex ;

public:
  MwM870* pIEC8705Export;
  MwM870* pIEC8705Import;

  BOOL    HaveImport;

	Iec104Chan() ;
	virtual ~Iec104Chan() ;

	BOOL IsConnectExchange( void );
	void ExportData(WORD Nasdu, TripleType Type, WORD Index, double Value, DWORD Status, LONGLONG Time );
	virtual void ImportData(WORD Nasdu, WORD Type, WORD Index, double Value, DWORD Status, LONGLONG Time )
	{	  GlobLogPrintf("-------> in Iec104Chan::ImportData (virtual)");
	}

	BOOL OpenExchange( const MwString& Signature );
	void CloseExchange( void ) { 
		DrvClear(); 
	}
	virtual void LocalOnConnect( void ) {
		GlobLogPrintf("-------> in Iec104Chan::link connected (virtual)");
	}
	virtual void LocalOnDisConnect( void ) {
		GlobLogPrintf("-------> in Iec104Chan::link disconnected (virtual)");
	}
	virtual void LocalChannelInit(const char * Message) {
		GlobLogPrintf("-------> in Iec104Chan::%s (virtual)", Message);
	}
	void RemoteChannelInit( void );

	int GetProtocolIDX( void ) { return MyProtocolIndex; }

	//	virtual BOOL HaveTU( int KP, int Adr ) { return FALSE; }

public:
	void CallLocalOnConnection(Iec104Link * p104LinkArg) {
	  Selection = p104LinkArg;
	  LocalOnConnect();
	  Selection = NULL;
	}

	void CallLocalOnDisConnection(Iec104Link * p104LinkArg) {
	  Selection = p104LinkArg;
	  LocalOnDisConnect();
	  Selection = NULL;
	}

	void CallLocalChannelInit(Iec104Link * p104LinkArg, const char * Message) {
	  Selection = p104LinkArg;
	  LocalChannelInit(Message);
	  Selection = NULL;
	}

	void CallImportData(MwExportElement104 * p) {
	  ImportData(p -> Nasdu, p -> DataType, p -> DataIndex, 
		     p -> Value, p ->Status, p ->Time);
	}

	int NumberOfLinks() { return List104.size(); }
	BOOL TcpLoad( const MwString& Config );
	BOOL TryAccept( void );
	BOOL TryConnect( void );
	void TerminateConnection( char* Message );
	BOOL TcpClear( void );
	int TestSendReady( void );
	BOOL IsServer() { return TcpIsServer; }
private:
	MwString NetAddress, PortAddress;
	MwString MyConfig;
	int NetPort;
	BOOL TcpIsOpen, TcpIsConnect;
private:
	SOCKET ClientSocket, ServerSocket;
	struct sockaddr_in* pSocketAddress;
	BOOL TcpIsServer;
};

#endif // !defined(AFX_IEC104CHAN_H__INCLUDED_)
