// Iec104Link.cpp: implementation of the Iec104Link class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

//#define printf GlobLogPrintf

extern void GlobLogPrintf( const char *fmt, ... );
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

#include <ostream>

#include "Iec104Chan.h"
#include "IEC101defs.h"

//#define GetProtocolIndex() (-123)

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#ifndef _MW_USE_SELECT_
const int DefinedPollTime = 0;		// timeout = 0 ms
#endif

static const int SendAllDataPeriod = 900000 ;

//----------------------------------------------------------------------------------
#define  _TCP_DEVICE_DEBUG_MESSAGE_

void Iec104Link::Transmit(Protocol_Packet *p_Packet) {
  //	cout << "Transmit : ";
  //^^^^^^^^^^^^^^^^^^^^
  unsigned char* pc = p_Packet -> Get_Body();
  int len  = p_Packet -> Get_Length();
  //------------------------------------------------------
  /*
    cout << "len = " << len << ":";
    for (int i = 0; i < 6; i++) cout << int(*(pc+i)) << ",";
    cout << ":";
    { for (int i = 6; i < len; i++) cout << int(*(pc+i)) << ","; }
    cout << endl;
  */
  //------------------------------------------------------
  //
  WriteTcpStream(ClientSocket, (char*)pc, len * 8);

  if( DoDebugPrint > 2 )
    {
      GlobLogPrintfE("[IEC104:%d] SND message l=%d:",
		     GetProtocolIndex(), len);
      for( int i=0; i<len; i++ )
	GlobLogPrintfN(" %02x",(((char*)pc)[i])&0xff );
      GlobLogPrintfN("\n");
    }
  //-------------------------------------------------
  {
    int MaxKP = p104Chan->pIEC8705Export->GetMaxKP();
    for( int kp=p104Chan->pIEC8705Export->GetFirstKP(); kp!=0; kp=p104Chan->pIEC8705Export->GetNextKP() )
      p104Chan->pIEC8705Export->IncrementKpCadrCounter( kp );
  }
  //-------------------------------------------------

}

void Iec104Link::Handle_Receive(unsigned char* buf, int lbuf) {
  unsigned char* pc = buf;
  //---------------------------
  /*
    int len;
    cout << "Handle_Receive : ";
    len  = lbuf;
    GlobLogPrintf("Iec104Link::Handle_Receive: len = %d", len);
    for (int i = 0; i < len; i++) cout << char(*(pc+i));
    cout << endl;
  */
  //---------------------------
  if (lbuf < 0) {
    TerminateConnection("Iec104Link::Handle_Receive - abort()");
    return;
    //----------------------------------------------------------------
    /*
      cout << "lbuf  = " << lbuf << ":";
      for (int i = 0; i < -lbuf; i++) cout << int(char(*(pc+i))) << ",";
      cout << endl;
    */
    //----------------------------------------------------------------
  } else {
    //----------------------------------------------------------------
    /*
      len  = lbuf;
      cout << "len = " << len << ":";
      for (int i = 0; i < len; i++) cout << int(char(*(pc+i))) << ",";
      cout << endl;
    */
    //----------------------------------------------------------------
    P104.ReadQue.Put(lbuf, buf);

    if( DoDebugPrint > 2 )
      {
	GlobLogPrintfE("[IEC104:%d] RCV message l=%d:",
		       GetProtocolIndex(), lbuf);
	for( int i=0; i<lbuf; i++ )
	  GlobLogPrintfN(" %02x",buf[i]&0xff);
	GlobLogPrintfN("\n");
      }
  }
}
//-------------------------------------------------------------------------------------
Iec104Link::Iec104Link(Iec104Chan * p104ChanArg, int IsServerArg, SOCKET ClientSocketArg)
{
  TcpIsConnect = FALSE;
  p104Chan = p104ChanArg;
  TcpIsServer =IsServerArg;
  ClientSocket = ClientSocketArg;
  SendAllDataTimer = 0;

  DoDebugPrint = 0;
  //GlobLogPrintf("Iec104Link::Iec104Link:ClientSocket = %d", ClientSocket);
}

Iec104Link::~Iec104Link()
{
  //	GlobLogPrintf("Iec104Link::~Iec104Link");
  if (!TcpIsServer) {
    p104Chan -> LocalOnDisConnect();
  }
  P104.DrvClear() ;
  TerminateConnection(NULL);
}

void Iec104Link::Init()
{
  TcpIsConnect = TRUE;
  Link2.Set_Upper_Layer(this);
  Link2.Set_Lower_Layer(this);

  P104.pIEC8705Export = p104Chan->pIEC8705Export;
  P104.pIEC8705Import = p104Chan->pIEC8705Import;
  P104.HaveImport     = p104Chan->HaveImport;
  P104.SetProtocolIndex( GetProtocolIndex() );
  //P104.SetDebugLevel(2);
  P104.DrvInit();
  Link2.Reset();
  if (!TcpIsServer) {
    Link2.STARTDTact();
    DataEnableFlag = TRUE;
    p104Chan -> LocalOnConnect();
  } else {
    DataEnableFlag = FALSE;
  }
  TESTFRactHasBeenSent = false;
}

BOOL Iec104Link::DrvDataExchange()
{
  enum {L_Buffer = 256};
  unsigned char Buffer[L_Buffer];
  int l;
  unsigned char * cPointer;
  long DataTimer;
  int  DecoderFrameCounter;
  if ( ! TcpIsConnect ) goto RETURN_FALSE;
  if (Link2.CheckErrorFlag())
    {
      TerminateConnection("IEC104 L2 error");
      goto RETURN_FALSE;
    }
  DecoderFrameCounter = 0;
  while (DecoderFrameCounter < 200)
    {
      l = ReadTcpStream(ClientSocket, (char *)Buffer, L_Buffer)  ;
      if( l <= 0 ) break;
      for (int i = 0; i < l; i++)
	{
	  //GlobLogPrintf("i, Buffer[i]  = %d, %d", i, Buffer[i]);
	  Decoder.PutNextByte(Buffer[i]);
	  if (Decoder.Error())
	    {
	      GlobLogPrintf("IEC104 RED ALERT");
	      TerminateConnection("IEC104 frame format error");
	      Decoder.Clear();
	      //-------------------------------------------------
	      {
		int MaxKP = p104Chan->pIEC8705Import->GetMaxKP();
		for( int kp=p104Chan->pIEC8705Import->GetFirstKP(); kp!=0; kp=p104Chan->pIEC8705Import->GetNextKP() )
		  p104Chan->pIEC8705Import->InvalidateKP( kp );
	      }
	      //-------------------------------------------------
	      goto RETURN_FALSE;
	    }
	  if (Decoder.NewFrameReady())
	    {
	      Link2.Handle_Receive(&Decoder.frame[0], Decoder.NumberOfBytes());
	      Decoder.Clear();
	      DecoderFrameCounter++;
	      //-------------------------------------------------
	      {
		int MaxKP = p104Chan->pIEC8705Import->GetMaxKP();
		for( int kp=p104Chan->pIEC8705Import->GetFirstKP(); kp!=0; kp=p104Chan->pIEC8705Import->GetNextKP() )
		  p104Chan->pIEC8705Import->IncrementKpCadrCounter( kp );
	      }
	      //-------------------------------------------------
	    }
	}
    }
  if (l < 0)
    {
      TerminateConnection("IEC104 read error");
      goto RETURN_FALSE;
    }
  {
    time_t t1 = Link2.GetTimer();
    time_t t2 = time(NULL);
    if (t2 > t1 + 15)
      {
	if (!TESTFRactHasBeenSent)
	  {
	    Link2.TESTFRact();
	    TESTFRactHasBeenSent = true;
	  }
      }
    else
      {
	TESTFRactHasBeenSent = false;
      }
    if (t2 > t1 + 20)
      {
	TerminateConnection("IEC104 timeout");
	goto RETURN_FALSE;
      }
    BOOL L2DataEnableFlag = Link2.CheckDataEnableFlag();
    if (TcpIsServer)
      {
	if (DataEnableFlag != L2DataEnableFlag)
	  {
	    if (DataEnableFlag)
	      {
		p104Chan -> CallLocalOnDisConnection(this);
	      }
	    else
	      {
		p104Chan -> CallLocalOnConnection(this);
	      }
	  }
	DataEnableFlag = L2DataEnableFlag;
      }
    if (!DataEnableFlag) return TRUE;
  }
  while ((l = P104.WriteQue.Get(cPointer)) >=0)
    {
      Protocol_Packet PP(l, 20);
      unsigned char* pc = PP.Get_Body();
      for (int i = 0; i < l; i++) *pc++ = *cPointer++;
      Link2.Transmit(&PP);
    }

  //  P104.DataRequestType = RF_SPORAD;

  P104.RunBalanced( TcpIsConnect );

  if (     P104.LocalChannelInit()  )
    {
      p104Chan -> CallLocalChannelInit(this, "LocalChannelInit");
      P104.DataRequestType = RF_FONE;
    }
  else if( P104.HaveDataRequest() )
    {
      p104Chan -> CallLocalChannelInit(this, "HaveDataRequest");
      P104.DataRequestType = RF_D_RQ ;
    }
  else
    {
      DataTimer = time(NULL);
      if (DataTimer > SendAllDataTimer + SendAllDataPeriod)
	{
	  SendAllDataTimer = DataTimer;
	  p104Chan -> CallLocalChannelInit(this, "SendAllData");
	  P104.DataRequestType = RF_FONE;
	}
    }

  unsigned char * pdata;
  while (P104.ControlQueSET.Get(pdata) > 0)
    {
      p104Chan -> CallImportData((MwExportElement104 *) pdata);
    }
  while (P104.StatusQue.Get(pdata) > 0)
    {
      p104Chan -> CallImportData((MwExportElement104 *) pdata);
    }
  while (P104.ImportQueTC.Get(pdata) > 0)
    {
      p104Chan -> CallImportData((MwExportElement104 *) pdata);
    }
  while (P104.ImportQueTI.Get(pdata) > 0)
    {
      p104Chan -> CallImportData((MwExportElement104 *) pdata);
    }
  return TRUE;

 RETURN_FALSE:
  if (TcpIsServer && DataEnableFlag) p104Chan -> CallLocalOnDisConnection(this);

  //-------------------------------------------------
  {
    int kp;
    /*
    for( kp=p104Chan->pIEC8705Export->GetFirstKP(); kp!=0; kp=p104Chan->pIEC8705Export->GetNextKP() )
      p104Chan->pIEC8705Export->InvalidateKP( kp );
    */
    for( kp=p104Chan->pIEC8705Import->GetFirstKP(); kp!=0; kp=p104Chan->pIEC8705Import->GetNextKP() )
      p104Chan->pIEC8705Import->InvalidateKP( kp );
  }
  //-------------------------------------------------

  return FALSE;
}

void Iec104Link::RemoteChannelInit()
{
  P104.RemoteChannelInit();
}

void Iec104Link::ExportData(WORD Nasdu, TripleType Type, WORD Index, double Value, DWORD Status, LONGLONG Time )
{
  if ( ! TcpIsConnect ) return;
  MwExportElement104 ExportElement;
  ExportElement.Nasdu = Nasdu; 
  ExportElement.DataIndex = Index; 
  ExportElement.Value = Value; 
  ExportElement.Status = Status; 
  ExportElement.Time = Time;
  ExportElement.EltType = Type;
  switch (Type)
    {
    case TripleTypeTC: 
      ExportElement.DataType = 1; 
      P104.ExportQueTC.Put(sizeof(MwExportElement104), (unsigned char *)&ExportElement);
      break;

    case TripleTypeTI: 
      ExportElement.DataType = 2; 
      P104.ExportQueTI.Put(sizeof(MwExportElement104), (unsigned char *)&ExportElement);
      break;

    case TripleTypeTU:
    case TripleTypeTW:
      ExportElement.DataType = 3;
      GlobLogPrintf("Iec104Link::ExportData--ControlQueGET.Put(Kp=%d Ob=%d v=%g S=%08x T=%d)",
		    Nasdu,Index,Value,Status,Type);
      P104.ControlQueGET.Put(sizeof(MwExportElement104), (unsigned char *)&ExportElement);
      break;

    default:
      break;
    }
}
//-----------------------------------------------------------------------------------------------------
void Iec104Link::TerminateConnection( char* Message )
{
  TcpIsConnect = FALSE;
  if( ClientSocket != INVALID_SOCKET )
    {
      shutdown( ClientSocket, 2 );
      closesocket( ClientSocket );
      ClientSocket = INVALID_SOCKET;
    }
  if( Message == NULL )
    return;
  GlobLogPrintf("Iec104Link::(TCP) Terminate, Protocol (%d) -->  %s", GetProtocolIndex(), Message );
}

int Iec104Link::ReadTcpStream(SOCKET ClientSocket, char* Buffer, int BuffSize )
{
  if( !TcpIsConnect )
    {
      return ErrorDeviceNoConnection;
    }
  if( BuffSize == 0 )
    {
      return 0;
    }
  if( ( BuffSize < 1 ) || ( Buffer == NULL ) )
    {
      return ErrorDeviceInvalidOperation;
    }
  int RecvLength = BuffSize;
  int RecvPosition = 0;
  while( RecvLength > RecvPosition )
    {
      //GlobLogPrintf("ClientSocket = %d", ClientSocket);
      int Ret = recv( ClientSocket, Buffer+RecvPosition, RecvLength-RecvPosition,  MSG_NOSIGNAL );
      if (0 == Ret) {
	GlobLogPrintf("(104) Receive EOF");
	TerminateConnection("(104) Closed Connection (RECV)");
	return ErrorDeviceNoConnection;
      }
      if( Ret > 0 )
	RecvPosition += Ret;
#ifdef _MW_WINDOWS_
      else if( ( Ret == SOCKET_ERROR ) && ( WSAGetLastError() == WSAEWOULDBLOCK ) )
#else
      else if( errno == EAGAIN )
#endif
	{
	  return RecvPosition;
	}
#ifndef _MW_WINDOWS_
      else if( errno == EINTR )
	{
	  //			GlobLogPrintf("(TCP) Receive EINTR [%d], Protocol --> %d ", (int) errno, GetProtocolIndex() );
	  return RecvPosition;
	}
#endif
      else
	{
	  GlobLogPrintf("(TCP:104) Receive Error [%d], Protocol --> %d ",
			(int) errno, GetProtocolIndex() );
	  TerminateConnection("(TCP:104) Lost Connection (RECV)");
	  return ErrorDeviceNoConnection;
	}
    }
  return RecvPosition;
}

int Iec104Link::WriteTcpStream(SOCKET ClientSocket, char* Buffer, int BitNumber )
{
  if( !TcpIsConnect )
    {
      return ErrorDeviceNoConnection;
    }
  int TmpSendState = TestSendReady();
  if( TmpSendState < 0 )
    {
      if( TmpSendState == ErrorDeviceTimeOut )
	TmpSendState = 0;
      return TmpSendState;
    }
  if( BitNumber == 0 )
    {
      return 0;
    }
  if( ( BitNumber < 1 ) || ( Buffer == NULL ) )
    {
      return ErrorDeviceInvalidOperation;
    }
  int ActualWrite = BitNumber / ConstByteSize;
  if( ActualWrite < 1 )
    {
      return 0;
    }
  //---------------------------------------------------------------------------------
  int SendResult = send( ClientSocket, Buffer, ActualWrite, MSG_NOSIGNAL );
  if( SendResult > 0 )
    {
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
      //		GlobLogPrintf("(TCP) Send OK [%d], Protocol --> %d ", SendResult, GetProtocolIndex() );
#endif
      return SendResult * ConstByteSize;
    }
  else if( SendResult == 0 )
    {
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
      GlobLogPrintf("(TCP:104) Send Empty, Protocol --> %d ", GetProtocolIndex() );
#endif
      return 0;
    }
#ifdef _MW_WINDOWS_
  else if( ( SendResult == SOCKET_ERROR ) && ( WSAGetLastError() == WSAEWOULDBLOCK ) )
#else
  else if( errno == EAGAIN )
#endif
    {
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
      GlobLogPrintf("(TCP:104) Send in Progress, Protocol --> %d ", GetProtocolIndex() );
#endif
      return 0;
    }
  TerminateConnection("(TCP:104) Lost Connection (SEND)");
  return ErrorDeviceNoConnection;
}

int Iec104Link::TestSendReady( void )
{
  if( !TcpIsConnect )
    {
      return ErrorDeviceNoConnection;
    }

#ifdef _MW_USE_SELECT_
  fd_set wds, exc;

  FD_ZERO( &wds );	FD_SET( ClientSocket, &wds );
  FD_ZERO( &exc );	FD_SET( ClientSocket, &exc );

  struct timeval TmpValTime;
  TmpValTime.tv_sec = 0;
  TmpValTime.tv_usec = 100;
  int iss = select( ClientSocket+1, NULL, &wds, &exc, &TmpValTime);
#else
  struct pollfd ufds;
  ufds.fd = ClientSocket;
  ufds.events = POLLOUT | POLLERR | POLLHUP;
  int iss = poll(&ufds, 1, DefinedPollTime );	
#endif
  if( iss == 0 )
    {
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
      GlobLogPrintf("(TCP:104) Select Timeout (SEND), Protocol --> %d ", GetProtocolIndex() );
#endif
      return ErrorDeviceTimeOut;
    }
  if(iss < 0)
    {
      TerminateConnection("(TCP:104) Cannot Send Data");
      return ErrorDeviceWrite;
    }

#ifdef _MW_USE_SELECT_
  if(  FD_ISSET( ClientSocket, &wds)  )
#else
    if( ufds.revents & POLLOUT )
#endif
      {
	int Val = 0;
	socklen_t LenVal = sizeof( Val );
	if( 0 == getsockopt(  ClientSocket, SOL_SOCKET, SO_ERROR,  (char*) &Val, &LenVal ) )
	  if( Val == 0 )
	    {
#ifdef  _TCP_DEVICE_DEBUG_MESSAGE_
	      //				GlobLogPrintf("(TCP:104) Send OK, Protocol --> %d ", GetProtocolIndex() );
#endif
	      return 0;
	    }
      }

  TerminateConnection("(TCP:104) Cannot Send Data (SELECT)");
  return ErrorDeviceWrite;
}

