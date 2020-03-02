//  $Id: rgsocket.cc 1.23 1998/05/27 16:58:58 hardy Exp hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@ibm.net.
//
//  This file is part of soup++ for OS/2.  Soup++ including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  simple socket class (everything is done in text mode)
//
//  attention:
//  ----------
//  TSocket::open requires semaphor support for multithreaded environment.
//  For OS/2 it's implemented, but not for any other os!
//


#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
////#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "rgmts.hh"
#include "rgsocket.hh"
#include "rgsema.hh"



#define OPENSOCKETSIZE   1024
#define CHECKABORT( action )      { if (socketAbort) { action } }
#define CHECKABORTANDKILL         { CHECKABORT( close(); raise(SIGUSR1); sleep(2); ) }

static unsigned long totalBytesRcvd = 0;
static unsigned long totalBytesXmtd = 0;
static char openSocket[OPENSOCKETSIZE];    // Holzhammer
static int  socketAbort = 0;

extern TSemaphor sysSema;



//--------------------------------------------------------------------------------



unsigned long _atoip( const char *hname )
{
    struct sockaddr_in ad;
    struct hostent *hostp;
    static TSemaphor sema;

    if (hname == NULL  ||  *hname == '\0')
	return INADDR_ANY;

    sema.Request();

    memset(&ad, 0, sizeof(ad));
    if ((ad.sin_addr.s_addr = inet_addr(hname)) == INADDR_NONE) {
	if ((hostp = gethostbyname(hname)) == NULL  &&  (hostp = gethostbyname(hname)) == NULL) {
	    errno = EINVAL;
	    sema.Release();
	    return INADDR_NONE;
	}
	if (hostp->h_addrtype != AF_INET) {
	    errno = EPROTOTYPE;
	    sema.Release();
	    return INADDR_NONE;
	}
	memcpy(&ad.sin_addr, hostp->h_addr, sizeof(ad.sin_addr));
    }
    
    sema.Release();
    return ad.sin_addr.s_addr;
}   // _atoip



//--------------------------------------------------------------------------------



TSocket::TSocket( void )
{
    State = init;
    sock = 0;
}   // TSocket::TSocket



TSocket::TSocket( int sock, const char *ipAdr, unsigned long ipNum,
		  int textMode, int buffSize )
{
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    State = init;
    
    TSocket::sock = sock;
    TSocket::ipAdr = xstrdup( ipAdr );
    TSocket::ipNum = ntohl(ipNum);

    //
    //  setup textmode (only applicable for read()/write()
    //
    if (textMode)
	setmode( sock, O_TEXT );
    else
	setmode( sock, O_BINARY );
    
    //
    //  setup buffer
    //
    Buffer = new unsigned char [buffSize+10];
    TSocket::BuffSize = buffSize;
    BuffNdx = BuffEnd = 0;

    bytesRcvd = bytesXmtd = 0;

    //
    //  store service&protocol (currently n.a.)
    //
    TSocket::service = xstrdup("");
    TSocket::protocol = xstrdup("");

    //
    //  connected!
    //
    State = connected;
    openSocket[sock] = 1;
}   // TSocket::TSocket



TSocket::~TSocket()
{
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    if (State == connected  ||  State == shutdwn)
	close();
}   // TSocket::~TSocket



unsigned long TSocket::getTotalBytesRcvd( void )
{
    return totalBytesRcvd;
}   // TSocket::getBytesRcvd



unsigned long TSocket::getTotalBytesXmtd( void )
{
    return totalBytesXmtd;
}   // TSocket::getBytesXmtd



const char *TSocket::getLocalhost( void )
//
//  Returns a pointer to the local host name.  The buffer for the name resides
//  on the heap and must be freed after usage...
//
{
    static TSemaphor sema;
    struct sockaddr_in local;
    int addrLen;
    struct hostent *hp;
    const char *localhost;
    char *buf;

#ifdef TRACE_ALL
    printfT("TSocket::getLocalHost()\n" );
#endif
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    sema.Request();
    addrLen = sizeof( local );
    getsockname( sock, (struct sockaddr *)&local, &addrLen );
    hp = gethostbyaddr( (const char *)&local.sin_addr, sizeof(local.sin_addr),
			AF_INET );
    localhost =  hp ? hp->h_name : inet_ntoa(local.sin_addr);
    buf = new char [strlen(localhost)+1];
    strcpy( buf,localhost );
    sema.Release();

    return buf;
}   // TSocket::getLocalhost



void TSocket::shutdown( int how )
{
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    sysSema.Request();
    if (State == connected) {
	::fcntl( sock, F_SETFL, O_NONBLOCK );
	::shutdown( sock,how );
	State = shutdwn;
    }
    sysSema.Release();
}   // TSocket::shutdown



void TSocket::abortAll( void )
{
    int c, h, res;

#ifdef TRACE
    printfT( "TSocket::abortAll()\n" );
#endif
    socketAbort = 1;

    for (h = 0;  h < OPENSOCKETSIZE;  ++h) {
        if (openSocket[h]) {
            sysSema.Request();
            if (openSocket[h]) {
                res = ::fcntl( h, F_SETFL, O_NONBLOCK );
#ifdef DEBUG
                printfT( "TSocket::abortAll(), fcntl: %d,%d\n",h,res );
#endif
            }
            sysSema.Release();
        }
    }

    for (c = 0;  c < 20;  ++c) {
	int found = 0;
	_sleep2( 100 );
        for (h = 0;  h < OPENSOCKETSIZE;  ++h) {
            if (openSocket[h]) {
                sysSema.Request();
                if (openSocket[h]) {
                    openSocket[h] = 0;
                    res = ::close( h );
#ifdef DEBUG
                    printfT( "TSocket::abortAll(), close: %d,%d\n",h,res );
#endif
                    found = 1;
                }
                sysSema.Release();
            }
	}
	if ( !found)
	    break;
    }
}   // TSocket::abortAll



void TSocket::close( void )
{
#ifdef TRACE_ALL
    printfT( "TSocket::close()\n" );
#endif
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    while (State == connecting) {
	CHECKABORT( break; );
	_sleep2( 100 );
    }

    sysSema.Request();
    if ((State == connected  ||  State == shutdwn)  &&  openSocket[sock]) {
#ifdef DEBUG_ALL
	printfT( "closing %s:%s/%s\n", ipAdr,service,protocol );
#endif
////	::shutdown( sock,2 );
	::close( sock );
	openSocket[sock] = 0;
	sock = 0;
	
	delete ipAdr;    ipAdr = NULL;
	delete service;  service = NULL;
	delete protocol; protocol = NULL;
	delete Buffer;   Buffer = NULL;
    }
    sysSema.Release();
    State = closed;
}   // TSocket::close



int TSocket::open( const char *ipAdr, const char *service, const char *protocol,
		   int portno, int textMode, int buffSize )
//
//  Socket ”ffnen:  Parameter sind wohl halbwegs klar...
//  portno <= 0 -> aus %ETC%/SERVICES den Port holen (ist portno angegeben, so sind
//                 service/protocol unwichtig)
//  - Es muá beim Return aufgepaát werden, daá State != connecting gesetzt wird - sonst
//    wartet der close() u.U. endlos...
//  - Open() muá im MT-Fall mit Semaphoren abgesichert werden, da diverse Socket-Fkts
//    Zeiger auf statische Struct zurckgeben!
//  - der einfachheithalber wird im Fehlerfall mit einem GOTO ans Ende gesprungen.  Dort
//    wird dann u.a. das Semaphor freigegeben
//  
//  Return: >=0 -> ok
//          < 0 -> failed (im Moment keine weiteren Angaben)
//
{
    int port, socktmp;
    struct sockaddr_in ad;
    static TSemaphor OpenSema;

#ifdef TRACE_ALL
    printfT( "TSocket::open(%s,%s,%s,%d,%d)\n",ipAdr,service,protocol,portno,buffSize );
#endif

    if (State != closed  &&  State != init)
	close();

    State = connecting;
    sock = 0;
    socktmp = -1;
    
    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = _atoip( ipAdr );

    //
    //  Service auseinanderfieseln und Port# bestimmen
    //
    if (portno > 0)
	port = htons( portno);
    else if (isdigit(service[0]))
	port = htons( atoi(service) );
    else {
	struct servent *serv;

	OpenSema.Request();
	serv = getservbyname (service,protocol);
	if (serv == NULL) {
	    OpenSema.Release();
	    goto OpenFailed;
	}
	port = serv->s_port;
	OpenSema.Release();
    }
    ad.sin_port = port;

    //
    //  Verbindung aufbauen
    //
    sysSema.Request();        // bug in emxrev <= 52:  socket()/open() etc are not threadsafe (_fd_init())
    socktmp = socket( AF_INET, SOCK_STREAM, 0 );
    sysSema.Release();
    if (socktmp < 0)
	goto OpenFailed;
    if (connect(socktmp,(struct sockaddr *)&ad,sizeof(ad)) < 0)
        goto OpenFailed;
    sock = socktmp;

    //
    //  setup textmode (only applicable for read()/write()
    //
    if (textMode)
	setmode( sock, O_TEXT );
    else
	setmode( sock, O_BINARY );
    
    //
    //  setup buffer
    //
    Buffer = new unsigned char [buffSize+10];
    TSocket::BuffSize = buffSize;
    BuffNdx = BuffEnd = 0;

    bytesRcvd = bytesXmtd = 0;
    
    //
    //  store ip&protocol
    //
    TSocket::ipAdr = xstrdup(ipAdr);
    TSocket::ipNum = ntohl(ad.sin_addr.s_addr);
    TSocket::service = xstrdup(service);
    TSocket::protocol = xstrdup(protocol);

    //
    //  connected!
    //
    assert( sock < OPENSOCKETSIZE );
    State = connected;
    openSocket[sock] = 1;
#ifdef DEBUG_ALL
    printfT( "socket opened successfully %s,%s/%s\n",
             TSocket::ipAdr,TSocket::service,TSocket::protocol );
#endif

    CHECKABORTANDKILL;
    return sock;
    
OpenFailed:
    if (socktmp > 0)
	::close( socktmp );
    sock = 0;
    State = closed;

    CHECKABORTANDKILL;
    return -1;
}   // TSocket::open



int TSocket::send( const void *src, int len )
{
    int n;
    char *buf = (char *)src;
    int res = len;

#ifdef DEBUG
    *(buf+len) = '\0';
#endif
#ifdef DEBUG_ALL
    printfT( "TSocket::send(%s,.): %d\n", buf,sock );
#endif
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    {
        blockThread();
        totalBytesXmtd += len;
        bytesXmtd += len;
        unblockThread();
    }
    while (len) {
	CHECKABORTANDKILL;
#ifdef DEBUG_ALL
	if (State != connected  ||  !openSocket[sock]) {
	    printfT( "TSocket::send():  socket closed\n" );
	    return -1;
	}
#endif
        n = ::write( sock, buf, len );
	CHECKABORTANDKILL;
        if (n <= 0) {
#ifdef DEBUG
	    printfT( "TSocket::send(): error '%s',%d,%d,%d\n", buf,sock,errno,n );
#endif
            return -1;
	}
        len -= n;
        buf += n;
    }
    return res;
}   // TSocket::send



int TSocket::printf( const char *fmt, ... )
{
    va_list ap;
    char buf[BUFSIZ];
    int res;

    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    res = -1;
    if (State == connected) {
	va_start( ap, fmt );
	vsprintfT( buf, fmt, ap );
	va_end( ap );
	res = send( buf, strlen(buf) );
    }
    return res;
}   // TSocket::printf



int TSocket::nextchar( void )
{
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    if (State != connected  ||  BuffEnd < 0) {
#ifdef DEBUG_ALL
	printfT( "TSocket::nextchar():  State %d, BuffEnd %d\n", State, BuffEnd );
#endif
	return -1;
    }

    if (BuffNdx >= BuffEnd) {
	CHECKABORTANDKILL;
#ifdef DEBUG_ALL
	if ( !openSocket[sock]) {
	    printfT( "TSocket::nextchar():  socket %d closed\n",sock );
	    return -1;
	}
#endif
	BuffEnd = ::read( sock, Buffer, BuffSize );
	CHECKABORTANDKILL;
	if (BuffEnd <= 0) {
#ifdef DEBUG_ALL
	    printfT( "TSocket::nextchar() error: %d,%d,%d\n",sock,errno,BuffEnd );
#endif
	    BuffEnd = -1;
	    return -1;
	}
#ifdef DEBUG_ALL
	printfT( "TSocket::nextchar():  %d,%d\n", sock,BuffEnd );
#endif
        BuffNdx = 0;
        {
            blockThread();
            totalBytesRcvd += BuffEnd;
            bytesRcvd += BuffEnd;
            unblockThread();
        }
    }
    return Buffer[BuffNdx++];
}   // TSocket::nextchar



int TSocket::recv( void *dst, int len )
{
    int r = -1;

#ifdef TRACE_ALL
    printfT( "TSocket::recv(.,%d)\n", len );
#endif
    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    if (State == connected) {
	if (BuffNdx < BuffEnd) {
	    r = BuffEnd-BuffNdx;
	    if (r > len)
		r = len;
	    memcpy( dst,Buffer+BuffNdx,r );
	    BuffNdx += r;
	}
	else {
	    r = ::read( sock,dst,len );
            if (r > 0) {
                blockThread();
                totalBytesRcvd += r;
                bytesRcvd += r;
                unblockThread();
            }
	}
    }
    return r;
}   // TSocket::recv



char *TSocket::gets( char *buff, int bufflen )
//
//  gets from socket ('\n' is not contained in return buff)
//  on EOF NULL is returned, otherwise buff
//  If line is too long to fit into buff, characters will be fetched til EOL
//
{
    char *p;     // Zeiger auf gelesene Zeichen
    int  n;      // Anzahl der gelesenen Zeichen
    int  c;      // gelesenes Zeichen

    assert( sock >= 0  &&  sock < OPENSOCKETSIZE );

    if (bufflen <= 1) {             // buff muá min. 2 Zeichen lang sein!
#ifdef DEBUG
	printfT( "TSocket::gets():  ill bufflen %d\n",bufflen );
#endif
	return NULL;
    }

    p = buff;
    n = 0;
    for (;;) {
	if (n >= bufflen-2) {       // if Buffer exhausted, read til '\n'
	    do
		c = nextchar();
	    while (c != '\n'  &&  c != -1);
	    break;
	}

	c = nextchar();
	if (c == -1) {              // EOF!
	    if (n == 0) {
		*p = '\0';
#ifdef DEBUG
		strcpy( buff,"!!EOF!!" );
		printfT( "TSocket::gets():  !!EOF!!\n" );
#endif
		return( NULL );
	    }
	    else
		break;
	}
	else if (c == '\r'  ||  c == '\0')         // skip '\r', '\0'
	    continue;
	else if (c == '\n')         // Zeile fertig !
	    break;
	else {
	    *(p++) = c;
	    ++n;
	}
    }
    *p = '\0';
#ifdef DEBUG_ALL
    printfT( "TSocket::gets(): %d,'%s'\n", sock,buff );
#endif
    return buff;
}   // TSocket::gets
