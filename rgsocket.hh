//  $Id: rgsocket.hh 1.13 1998/04/12 16:23:32 hardy Exp hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@ibm.net.
//
//  This file is part of soup++ for OS/2.  Soup++ including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __RGSOCKET_HH__
#define __RGSOCKET_HH__


unsigned long _atoip( const char *hname );


class TSocket {
public:
    enum TState {init,connecting,connected,shutdwn,closed};

protected:
    int sock;
    
private:
    TState State;
    unsigned char *Buffer;
    int  BuffSize;
    int  BuffEnd;
    int  BuffNdx;
    unsigned long bytesRcvd;
    unsigned long bytesXmtd;
    const char *ipAdr;
    unsigned long ipNum;
    const char *service;
    const char *protocol;

    int nextchar( void );

public:
    TSocket( void );
    TSocket( int sock, const char *ipAdr, unsigned long ipNum,
	     int textMode=1, int buffSize=4096 );
    TSocket( const TSocket &right );     // copy constructor not allowed !
    ~TSocket();
    operator = (const TSocket &right);   // assignment operator not allowed !

    void close( void );
    int open( const char *ipAdr, const char *service, const char *protocol,
	      int port=-1, int textMode=1, int buffSize=4096 );
    int printf( const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
    char *gets( char *buff, int bufflen );

    int send( const void *src, int len );
    int recv( void *dst, int len );
    void shutdown( int how=2 );

    TState state( void ) { return State; }
    const char *getIpAdr( void ) { return ipAdr; }
    unsigned long getIpNum( void ) { return ipNum; }
    const char *getLocalhost( void );

    int getfd( void ) { return sock; }   // only temp!
    
    unsigned long getBytesRcvd( void ) { return bytesRcvd; }
    unsigned long getBytesXmtd( void ) { return bytesXmtd; }
    static unsigned long getTotalBytesRcvd( void );
    static unsigned long getTotalBytesXmtd( void );
    static void abortAll( void );
};


#endif
