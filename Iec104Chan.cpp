// Iec104Chan.cpp: implementation of the Iec104Chan class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

extern void GlobLogPrintfN( const char *fmt, ... );

#include "StdAfx.h"
#ifdef _MW_WINDOWS_
#include "ProtMon.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <sys/poll.h>
#endif

#include <fcntl.h>
#include <sys/types.h>

#include "Iec104Chan.h"
#include "MwConst.h"
//#define GetProtocolIndex() ( MyProtocolIndex )

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#ifndef _MW_USE_SELECT_
const int DefinedPollTime = 0;		// timeout = 0 ms
#endif

//- #define  _TCP_DEVICE_DEBUG_MESSAGE_ //VK

const int DefaultHostNameLength = 400;
const DWORD TcpDeviceReadWaitPeriod = 400;
const DWORD ConnectTcpWaitPeriod = 400;

Iec104Chan::Iec104Chan()
{
	TcpIsServer = FALSE;
	pLink104Client = NULL;
	HaveImport = FALSE;
}

Iec104Chan::~Iec104Chan()
{
	if (TcpIsServer) {
		list<Iec104Link *>::iterator pList104 = List104.begin();
		while (pList104 != List104.end()) {
			Iec104Link * pLink104 = * pList104;
			delete pLink104;
			pList104 = List104.erase(pList104);
			GlobLogPrintf("~Iec104Chan: Number of clients = %d", List104.size());
		}
	} else {
		if (pLink104Client) delete pLink104Client;
		pLink104Client = NULL;
	}
	if (pSocketAddress) delete pSocketAddress;
}

BOOL Iec104Chan::OpenExchange( const MwString& Config )
{
  GlobLogPrintf("[%d]Iec104Chan::OpenExchange - Config:%s",
		MyProtocolIndex, (const char*)Config);//VK
  ChannelConfig = Config;
  MwString TmpSignature = Config;
  TmpSignature.TrimLeft();
  TmpSignature.TrimRight();
  TmpSignature.MakeLower();

  if ( ! TcpLoad(TmpSignature)) {
	return FALSE;
  }
  pLink104Client = NULL;
  return TRUE;
}

BOOL Iec104Chan::DrvClear( void )
{
	GlobLogPrintf("MwServer104Protocol::DrvClear");
	return TRUE;
}

BOOL Iec104Chan::DrvStop( void )
{
	GlobLogPrintf("MwServer104Protocol::DrvStop");
	return TRUE;
}


//--------------------------------------------------------------------------------------------------

void Iec104Chan::RemoteChannelInit()
{
	if( !TcpIsOpen ) return;

//	GlobLogPrintf("Iec104Chan::RemoteChannelInit");
	if (TcpIsServer) {
		list<Iec104Link *>::iterator pList104 = List104.begin();
		while (pList104 != List104.end()) {
			Iec104Link * pLink104 = * pList104;
			pLink104 -> RemoteChannelInit();
			pList104++;
		}
	} else {
		pLink104Client -> RemoteChannelInit();
	}
}

BOOL Iec104Chan::IsConnectExchange()
{
	if( !TcpIsOpen ) return FALSE;

	if( TcpIsServer ) {
		int TcpNewConnection = TryAccept();
		if (TcpNewConnection) {
			Iec104Link * pLink104 = new Iec104Link(this, TcpIsServer, ClientSocket);
			pLink104 -> SetProtocolIndex( MyProtocolIndex );
			pLink104 -> Load( ChannelConfig );
			pLink104 -> Init();
			List104.push_back(pLink104);
			GlobLogPrintf("[iec104] Number of clients = %d", List104.size());
		}
		list<Iec104Link *>::iterator pList104 = List104.begin();
		while (pList104 != List104.end()) {
			Iec104Link * pLink104 = * pList104;
			if (! pLink104 -> DrvDataExchange()) {
				delete pLink104;
				pList104 = List104.erase(pList104);
				GlobLogPrintf("[iec104] Number of clients = %d", List104.size());
			} else {
				pList104++;
			}
		}
		return (List104.size() != 0);
	} else {
		if (!TcpIsConnect) TcpIsConnect = TryConnect();
		if (! TcpIsConnect) return FALSE;
		if (pLink104Client == NULL) {
			pLink104Client = new Iec104Link(this, TcpIsServer, ClientSocket);
			pLink104Client -> SetProtocolIndex( MyProtocolIndex );
			pLink104Client -> Load( ChannelConfig );
			pLink104Client -> Init();
		}
		if (! pLink104Client -> DrvDataExchange()) {
			TerminateConnection("Iec104Link disconnected");
			delete pLink104Client;
			pLink104Client = NULL;
			return FALSE;
		}
		return TRUE;
	}
}

void Iec104Chan::ExportData(WORD Nasdu, TripleType Type, WORD Index, double Value, DWORD Status, LONGLONG Time )
{
	if( !TcpIsOpen ) return;
	if( !TcpIsConnect) return;

//+ GlobLogPrintf("Iec104Chan::ExportData: Nasdu, Type, Index, Value, Status = %d, %d, %d, %f, %x", Nasdu, Type, Index, Value, Status);
	if (TcpIsServer) {
		list<Iec104Link *>::iterator pList104 = List104.begin();
		while (pList104 != List104.end()) {
			Iec104Link * pLink104 = * pList104;
			if (Selection == NULL || pLink104 == Selection) 
				pLink104 -> ExportData(Nasdu, Type, Index, Value, Status, Time );
			pList104++;
		}
	} else {
//GlobLogPrintf("Iec104Chan::ExportData: Nasdu, Type, Index, Value, Status = %d, %d, %d, %f, %x", Nasdu, Type, Index, Value, Status);
//GlobLogPrintf("Iec104Chan::pLink104Client = %x", pLink104Client);
		pLink104Client -> ExportData(Nasdu, Type, Index, Value, Status, Time );
	}
	return;
}

//-------------------------------------------------------------------------------------

static BOOL SetBlocking( SOCKET BlockSocket, BOOL BlockMode = FALSE )
{
	if( BlockSocket == INVALID_SOCKET )
		return FALSE;
#ifdef _MW_WINDOWS_
	u_long argp = 0;
	if( !BlockMode )
   	argp = 1;
	if( 0 != ioctlsocket( BlockSocket, FIONBIO, &argp ) )
		return FALSE;
#else
	int flags = fcntl( BlockSocket, F_GETFL, 0);
	if(flags == -1)
		return FALSE;
	if( BlockMode )
		flags = flags & (~O_NONBLOCK);
	else
		flags = flags | O_NONBLOCK;
	if( -1 == fcntl( BlockSocket, F_SETFL, flags ) )
		return FALSE;
#endif
	return TRUE;
}

BOOL Iec104Chan::TcpLoad( const MwString& Config )
{ 
	NetAddress.Empty();
	PortAddress.Empty();
	NetPort = 0;
	TcpIsOpen = FALSE;
	TcpIsConnect = FALSE;
	TcpIsServer = FALSE;
	ClientSocket = INVALID_SOCKET;
	ServerSocket = INVALID_SOCKET;
	pSocketAddress = NULL;

	MwString TmpSignature = Config;
	TmpSignature.TrimLeft();
	TmpSignature.TrimRight();
	TmpSignature.MakeLower();

	int Pos = TmpSignature.Find(':');
	if( Pos < 0 )
	{
		GlobLogPrintf("(TCP:104) Invalid Port number, Protocol --> %d ", GetProtocolIDX() );
		return FALSE;
	}
	int Size = TmpSignature.GetLength();
	NetAddress = TmpSignature.Left( Pos );
		NetAddress.TrimLeft();
		NetAddress.TrimRight();

	TmpSignature = TmpSignature.Right( Size - Pos - 1 );
	Pos = TmpSignature.Find('!');
	if( Pos > 0 )
	{
		Size = TmpSignature.GetLength();
		PortAddress = TmpSignature.Right( Size - Pos - 1 );
			PortAddress.TrimLeft();
			PortAddress.TrimRight();
		TmpSignature = TmpSignature.Left( Pos );
	}

	if( sscanf( TmpSignature, "%d", &NetPort ) != 1 )
	{
		GlobLogPrintf("(TCP:104) Invalid Port number, Protocol --> %d ", GetProtocolIDX() );
		return FALSE;
	}
	if( ( NetPort < 1 ) || ( NetPort > 0xFFFF ) )
	{
		GlobLogPrintf("(TCP:104) Invalid Port number, Protocol --> %d ", GetProtocolIDX() );
		return FALSE;
	}
	MwString HostName = NetAddress;
	GlobLogPrintf("(TCP:104) Load - Host:%s, Port:%d, IP:%s, Protocol --> %d ",
		(const char*)NetAddress,NetPort,
		(const char*)PortAddress,GetProtocolIDX() ); //VK
	if( NetAddress.IsEmpty() )
	{
		TcpIsServer = TRUE;
		if( !PortAddress.IsEmpty() )
			HostName = PortAddress;
		else
		{
			char HostNameBuffer[DefaultHostNameLength+1];
			gethostname( HostNameBuffer, DefaultHostNameLength );
			HostNameBuffer[DefaultHostNameLength] = 0;
			HostName = MwString( (const char*) HostNameBuffer );
		}
	}

	{
		struct hostent* pHostent = NULL;
		pHostent = gethostbyname( (const char*) HostName );
		if( pHostent == NULL )
		{
			GlobLogPrintf("(TCP:104) Host Unknown <%s>, Protocol --> %d ",
				      (const char*) HostName, GetProtocolIDX() );
			return FALSE;
		}
		pSocketAddress = new struct sockaddr_in;
		if( pSocketAddress == NULL )
		{
			GlobLogPrintf("(TCP:104) Cannot allocate memory for 'struct sockaddr_in', Protocol --> %d ",
				      GetProtocolIDX() );
			return FALSE;
		}
		memset(pSocketAddress, 0, sizeof(sockaddr_in));
		pSocketAddress->sin_family = AF_INET;
		pSocketAddress->sin_port = htons( NetPort );
//191005mp
		if( (TcpIsServer == TRUE) && (PortAddress.IsEmpty()) )
			pSocketAddress->sin_addr.s_addr = htonl(INADDR_ANY);
		else
//191005end
		memcpy( &(pSocketAddress->sin_addr), pHostent->h_addr, pHostent->h_length );
	}

	TcpIsOpen = TRUE;

	GlobLogPrintf("=======--->> Iec104Chan: open OK as `%s', Protocol --> %d",
		      (TcpIsServer)?"SERVER":"CLIENT", GetProtocolIDX());

	return TRUE;
}

BOOL Iec104Chan::TryAccept( void )
{
	if( ServerSocket == INVALID_SOCKET )
	{
		ServerSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
#ifdef _MW_WINDOWS_
		if( ServerSocket == INVALID_SOCKET)
#else
		if( ServerSocket < 0)
#endif
		{
			ServerSocket = INVALID_SOCKET;
			GlobLogPrintf("(TCP:104) Cannot Create Socket, Protocol --> %d ", GetProtocolIDX() );
			return FALSE;
		}
		int iEnableFlag = 1;
		setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, (char *) &iEnableFlag, sizeof(int) );
		int BindResult = bind( ServerSocket, (sockaddr*) pSocketAddress, sizeof( *pSocketAddress ) );
		if( BindResult < 0 )
		{
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
			TerminateConnection("(TCP:104) Cannot Bind Socket");
#else
			TerminateConnection( NULL);
#endif
			GlobLogPrintf("(TCP:104) Cannot Bind Socket, Protocol --> %d ", GetProtocolIDX() );
			return FALSE;
		}
		SetBlocking( ServerSocket, FALSE );
		if( 0 != listen( ServerSocket, 5 ) )
		{
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
			TerminateConnection("(TCP:104) Cannot Listen Socket");
#else
			TerminateConnection( NULL);
#endif
			return FALSE;
		}
	}
//------------------------------------
	ClientSocket = accept( ServerSocket, NULL, NULL );
#ifdef _MW_WINDOWS_
	if( ClientSocket == INVALID_SOCKET)
#else
	if( ClientSocket < 0)
#endif
	{
		ClientSocket = INVALID_SOCKET;
		return FALSE;
	}

	SetBlocking( ClientSocket, FALSE );
	{
		shutdown( ServerSocket, 2 );
		closesocket( ServerSocket );
		ServerSocket = INVALID_SOCKET;
	}
	GlobLogPrintf("(TCP:104) Connect, Protocol --> %d ", GetProtocolIDX() );
	TcpIsConnect = TRUE;
	return TRUE;
}

BOOL Iec104Chan::TryConnect( void )
{
	if( ClientSocket == INVALID_SOCKET )
	{
		ClientSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
#ifdef _MW_WINDOWS_
		if( ClientSocket == INVALID_SOCKET)
#else
		if( ClientSocket < 0)
#endif
		{
			ClientSocket = INVALID_SOCKET;
			GlobLogPrintf("(TCP:104) Cannot Create Socket, Protocol --> %d ", GetProtocolIDX() );
			return FALSE;
		}
//+		GlobLogPrintf("(TCP) Create Socket:%d, Protocol --> %d ", ClientSocket, GetProtocolIDX() );
		int temp = SetBlocking( ClientSocket, FALSE );
//GlobLogPrintf("after SetBlocking: temp = %d", temp);
	}
		{
		int ConnectResult = connect( ClientSocket, (sockaddr*) pSocketAddress, sizeof( *pSocketAddress ) );
		if( ConnectResult >= 0 )
		{
			GlobLogPrintf("(TCP:104) Connect, Protocol --> %d ", GetProtocolIDX() );
			TcpIsConnect = TRUE;
			GlobLogPrintf("Iec104Chan::TryConnect:ClientSocket = %d", ClientSocket);
			return TRUE;
		}
#ifdef _MW_WINDOWS_
//GlobLogPrintf("WSAEWOULDBLOCK = %x", WSAEWOULDBLOCK);
//GlobLogPrintf("WSAEINVAL = %x", WSAEINVAL);
//GlobLogPrintf("WSAEALREADY = %x", WSAEALREADY);
//GlobLogPrintf("After connect: %x %x %x", ConnectResult, SOCKET_ERROR, WSAGetLastError());
		if( ( ConnectResult == SOCKET_ERROR ) && 
			( WSAGetLastError() != WSAEWOULDBLOCK ) &&
			( WSAGetLastError() != WSAEINVAL ) &&
			( WSAGetLastError() != WSAEALREADY))
#else
		if( errno != EINPROGRESS )
#endif
		{
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
			TerminateConnection("(TCP:104) Cannot Connect");
#else
			TerminateConnection( NULL);
#endif
	if( ClientSocket != INVALID_SOCKET )
	{
		shutdown( ClientSocket, 2 );
		closesocket( ClientSocket );
		ClientSocket = INVALID_SOCKET;
	}
			return FALSE;
		}
	}
//------------------------------------
	{
#ifdef _MW_USE_SELECT_
		fd_set wds, rds, exc;

      	FD_ZERO( &rds );	//FD_SET( ClientSocket, &rds );
      	FD_ZERO( &wds );	FD_SET( ClientSocket, &wds );
    	FD_ZERO( &exc );	//FD_SET( ClientSocket, &exc );

		struct timeval TmpValTime;
		TmpValTime.tv_sec = 0;
		TmpValTime.tv_usec = 100000;
	   	int iss = select( ClientSocket+1, &rds, &wds, &exc, &TmpValTime);
//GlobLogPrintf("After select: %x %x %x", iss, SOCKET_ERROR, WSAGetLastError());
#else
		struct pollfd ufds;
		ufds.fd = ClientSocket;
		ufds.events = POLLIN | POLLPRI | POLLOUT | POLLERR;
		int iss = poll(&ufds, 1, DefinedPollTime );
#endif
		if( iss == 0 )
		{
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
			GlobLogPrintf("(TCP:104) Select Timeout, Protocol --> %d ", GetProtocolIDX() );
#endif
				return FALSE;
			}
		if(iss < 0)
			return FALSE;

#ifdef _MW_USE_SELECT_
		if( FD_ISSET( ClientSocket, &rds) || FD_ISSET( ClientSocket, &wds)  )
#else
//		if( ufds.revents & ( POLLIN | POLLPRI | POLLOUT ) )
		if( ufds.revents & ( POLLOUT ) )
#endif
		{
			int Val = 0;
			socklen_t LenVal = sizeof( Val );
			if( 0 == getsockopt(  ClientSocket, SOL_SOCKET, SO_ERROR, (char*) &Val, &LenVal ) )
				if( Val == 0 )
				{
					GlobLogPrintf("(TCP:104) Connect, Protocol --> %d ", GetProtocolIDX() );
					TcpIsConnect = TRUE;
					return TRUE;
				}
		}

#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
		TerminateConnection("(TCP:104) Cannot Connect");
#else
		TerminateConnection( NULL);
#endif
	if( ClientSocket != INVALID_SOCKET )
	{
		shutdown( ClientSocket, 2 );
		closesocket( ClientSocket );
		ClientSocket = INVALID_SOCKET;
	}
	}
	return FALSE;
}

void Iec104Chan::TerminateConnection( char* Message )
{
	TcpIsConnect = FALSE;
	if( ServerSocket != INVALID_SOCKET )
	{
		shutdown( ServerSocket, 2 );
		closesocket( ServerSocket );
		ServerSocket = INVALID_SOCKET;
	}
	if( Message == NULL )
		return;
	GlobLogPrintf("(TCP:104) Terminate, Protocol (%d) -->  %s", GetProtocolIDX(), Message );
}
