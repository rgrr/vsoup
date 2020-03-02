/* $Id: pop3.cc 1.24 1999/08/29 13:12:50 Hardy Exp Hardy $
 *
 * This module has been modified for souper.
 */

/* Copyright 1993,1994 by Carl Harris, Jr.
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Carl Harris <ceharris@vt.edu>
 */

//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  NNTP client routines
//

/***********************************************************************
  module:       pop3.c
  program:      popclient
  SCCS ID:      @(#)pop3.c      2.4  3/31/94
  programmer:   Carl Harris, ceharris@vt.edu
  date:         29 December 1993
  compiler:     DEC RISC C compiler (Ultrix 4.1)
  environment:  DEC Ultrix 4.3 
  description:  POP2 client code.
 ***********************************************************************/


#include <assert.h> 
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <rgfile.hh>
#include <rgmts.hh>
#include <rgsocket.hh>
#include <md5.hh>

#include "areas.hh"
#include "global.hh"
#include "output.hh"
#include "pop3.hh"
#include "util.hh"



/* exit code values */

enum PopRetCode {ps_success,      // successful receipt of messages
                 ps_socket,       // socket I/O woes
                 ps_protocol,     // protocol violation
                 ps_error         // some kind of POP3 error condition
};


static long pop3TotalSize = 0;
static char sPercent[20];


/*********************************************************************
  function:      POP3_ok
  description:   get the server's response to a command, and return
                 the extra arguments sent with the response.
  arguments:     
    argbuf       buffer to receive the argument string (==NULL -> no return)
    socket       socket to which the server is connected.

  return value:  zero if okay, else return code.
  calls:         SockGets
 *********************************************************************/

static PopRetCode POP3_ok(char *argbuf, TSocket &socket) /*FOLD00*/
{
    PopRetCode ok;
    char buf[BUFSIZ];
    char *bufp;

#ifdef TRACE
    printfT( "POP3_ok()\n" );
#endif
    if (socket.gets(buf, sizeof(buf))) {
#ifdef DEBUG
	printfT( "POP3_ok() -> %s\n",buf );
#endif
	bufp = buf;
	if (*bufp == '+' || *bufp == '-')
	    bufp++;
	else
	    return(ps_protocol);

	while (isalpha(*bufp))
	    bufp++;
	*(bufp++) = '\0';
	
	if (strcmp(buf,"+OK") == 0)
	    ok = ps_success;
	else if (strcmp(buf,"-ERR") == 0)
	    ok = ps_error;
	else
	    ok = ps_protocol;
	
	if (argbuf != NULL)
	    strcpy(argbuf,bufp);
    }
    else {
	ok = ps_socket;
	if (argbuf != NULL)
	    *argbuf = '\0';
    }
    
    return(ok);
}   // POP3_ok



/*********************************************************************
  function:      POP3_Auth
  description:   send the USER and PASS commands to the server, and
                 get the server's response.
  arguments:     
    userid       user's mailserver id.
    password     user's mailserver password.
    socket       socket to which the server is connected.

  return value:  non-zero if success, else zero.
  calls:         SockPrintf, POP3_ok.
 *********************************************************************/

static PopRetCode POP3_Auth(const char *userid, const char *password, TSocket &socket)  /*FOLD00*/
{
    PopRetCode ok;
    char buf[BUFSIZ];

#ifdef TRACE
    printfT( "POP3_Auth(%s,.,.)\n", userid);
#endif
    socket.printf("USER %s\n",userid);
    if ((ok = POP3_ok(buf,socket)) == ps_success) {
	socket.printf("PASS %s\n",password);
	if ((ok = POP3_ok(buf,socket)) == ps_success) 
	    ;  //  okay, we're approved.. 
	else
	    areas.mailPrintf1( OW_MAIL,"%s\n",buf);
    }
    else
	areas.mailPrintf1(OW_MAIL,"%s\n",buf);
    
    return(ok);
}   // POP3_Auth



/*********************************************************************
  function:      POP3_ApopAuth
  description:   send the APOP command to the server, and
                 get the server's response.
  arguments:     
    userid       user's mailserver id.
    password     user's mailserver password.
    timestamp    mailserver's timestamp string.
    socket       socket to which the server is connected.

  return value:  non-zero if success, else zero.
  calls:         SockPrintf, POP3_ok.
 *********************************************************************/

static PopRetCode POP3_ApopAuth(const char *userid, const char *password, /*FOLD00*/
				const char *timestamp, int timestamplen,
				TSocket &socket) 
{
    PopRetCode ok;
    char buf[BUFSIZ];
    MD5_CTX mdContext;
    unsigned char mdBuffer[512];
    unsigned char digest[16];
    char acDigest[33];
    int	 i;

#ifdef TRACE
    printfT( "POP3_ApopAuth(%s,.,.)\n", userid);
#endif
    strncpy( (char *)mdBuffer, timestamp, timestamplen );
    strcpy( (char *)mdBuffer+timestamplen, password );
#ifdef TRACE_ALL
    printfT( "\nDigest for =%s\n", mdBuffer );
#endif
    
    /* digest data in 512 byte blocks */
    MD5Init( &mdContext );
    MD5Update( &mdContext, mdBuffer, strlen((char *)mdBuffer) );
    MD5Final( digest, &mdContext );
    for(i = 0;  i < 16;  i++)
	sprintfT( acDigest+2*i, "%02x", digest[i] );

#ifdef TRACE_ALL
    printfT( "is =%s\n", acDigest );
#endif
	
    socket.printf("APOP %s %s\n",userid,acDigest);
    if ((ok = POP3_ok(buf,socket)) == ps_success)
	areas.mailPrintf1( OW_MAIL,"APOP approved...\n" );
    else
	areas.mailPrintf1( OW_MAIL,"%s\n",buf);
    
    return ok;
}   // POP3_ApopAuth



/*********************************************************************
  function:      POP3_sendQuit
  description:   send the QUIT command to the server and close 
                 the socket.

  arguments:     
    socket       socket to which the server is connected.

  return value:  none.
  calls:         SockPuts, POP3_ok.
 *********************************************************************/

static PopRetCode POP3_sendQuit(TSocket &socket) /*FOLD00*/
{
    char buf[BUFSIZ];
    PopRetCode ok;

#ifdef TRACE
    printfT( "POP3_sendQuit(): QUIT\n" );
#endif
    socket.printf("QUIT\n");
    ok = POP3_ok(buf,socket);
    if (ok != ps_success)
	areas.mailPrintf1( OW_MAIL,"%s\n",buf);

    return(ok);
}   // POP3_sendQuit



/*********************************************************************
  function:      POP3_sendStat
  description:   send the STAT command to the POP3 server to find
                 out how many messages are waiting.
  arguments:     
    count        pointer to an integer to receive the message count.
    socket       socket to which the POP3 server is connected.

  return value:  return code from POP3_ok.
  calls:         POP3_ok, SockPrintf
 *********************************************************************/

static PopRetCode POP3_sendStat(int *msgcount, TSocket &socket, int displayStatus) /*FOLD00*/
{
    PopRetCode ok;
    char buf[BUFSIZ];
    long totalsize;
    static int firstCall = 1;
    
    socket.printf("STAT\n");
    ok = POP3_ok(buf,socket);
    if (ok == ps_success) {
	sscanfT(buf,"%d %ld",msgcount,&totalsize);
	//
	// show them how many messages we'll be downloading
	//
	if (firstCall) {
	    areas.mailPrintf1( OW_MAIL,"you have %d mail message%s\n", *msgcount,
			       (*msgcount == 1) ? "" : "s");
	    if (displayStatus)
                areas.mailPrintf1( OW_MAIL,"total message size: %ld bytes\n",totalsize );
            pop3TotalSize = totalsize;
	}
	firstCall = 0;
    }
    else
	areas.mailPrintf1( OW_MAIL,"%s\n",buf);

#ifdef DEBUG
    printfT( "POP3_sendStat: %d, %ld\n", *msgcount,totalsize );
#endif

    return(ok);
}   // POP3_sendStat




/*********************************************************************
  function:      POP3_sendList
  description:   send the LIST command to the POP3 server to find
                 out the actual message size
  arguments:     
    msgSize      pointer to a long to receive the message size.
    msgNumber    message number to retrieve the size
    socket       socket to which the POP3 server is connected.

  return value:  return code from POP3_ok.
  calls:         POP3_ok, SockPrintf
 *********************************************************************/

static PopRetCode POP3_sendList(long *msgSize, int msgNumber, TSocket &socket)
{
    PopRetCode ok;
    char buf[BUFSIZ];
    long msgCount;
    
    socket.printf("LIST %d\n",msgNumber);
    ok = POP3_ok(buf,socket);
    if (ok == ps_success) {
	sscanfT(buf,"%ld %ld",&msgCount,msgSize);
    }
    else
	areas.mailPrintf1( OW_MAIL,"%s\n",buf);

#ifdef DEBUG
    printfT( "POP3_sendList: %ld\n", *msgSize );
#endif

    return ok;
}   // POP3_sendList




/*********************************************************************
  function:      POP3_sendRetr
  description:   send the RETR command to the POP3 server.
  arguments:     
    msgnum       message ID number
    socket       socket to which the POP3 server is connected.

  return value:  return code from POP3_ok.
  calls:         POP3_ok, SockPrintf
 *********************************************************************/

static PopRetCode POP3_sendRetr(int msgnum, TSocket &socket) /*FOLD00*/
{
    PopRetCode ok;
    char buf[BUFSIZ];

#ifdef TRACE
    printfT( "POP3_sendRetr(%d,.)\n",msgnum );
#endif
    socket.printf("RETR %d\n",msgnum);
    ok = POP3_ok(buf,socket);
    if (ok != ps_success)
	areas.mailPrintf1( OW_MAIL,"%s\n",buf);

    return(ok);
}   // POP3_sendRetr



/*********************************************************************
  function:      POP3_sendDele
  description:   send the DELE command to the POP3 server.
  arguments:     
    msgnum       message ID number
    socket       socket to which the POP3 server is connected.

  return value:  return code from POP3_ok.
  calls:         POP3_ok, SockPrintF.
 *********************************************************************/

static PopRetCode POP3_sendDele(int msgnum, TSocket &socket) /*FOLD00*/
{
    PopRetCode ok;
    char buf[BUFSIZ];

#ifdef TRACE
    printfT( "POP3_sendDele(%d,.)\n", msgnum );
#endif
    socket.printf("DELE %d\n",msgnum);
    ok = POP3_ok(buf,socket);
    if (ok != ps_success)
	areas.mailPrintf1(OW_MAIL,"%s\n",buf);

    return(ok);
}   // POP3_sendDele



/*********************************************************************
  function:      POP3_readmsg
  description:   Read the message content as described in RFC 1225.
                 RETR with reply evaluation has been done before
  arguments:     
    socket       ... to which the server is connected.
    mboxfd       open file descriptor to which the retrieved message will
                 be written.  
    topipe       true if we're writing to the system mailbox pipe.

  return value:  zero if success else PS_* return code.
  calls:         SockGets.
 *********************************************************************/

static PopRetCode POP3_readmsg(TSocket &socket, TFile &outf, long msgSize) /*FOLD00*/
{
    char buf[BUFSIZ];
    char *bufp;
    static long bytesRead = 0;
    long oldKbRead;
    int inHeader = 1;
    int firstFromTo = 1;

    //
    //  read the message content from the server
    //
    oldKbRead = -1;
    outf.seek(0L,SEEK_SET);
    for (;;) {
	if (socket.gets(buf,sizeof(buf)) == NULL) {
	    return ps_socket;
	}
	bytesRead += strlen(buf) + 2;

        bufp = buf;
	if (*bufp == '\0')
	    inHeader = 0;
	else if (*bufp == '.') {
	    bufp++;
	    if (*bufp == 0)
		break;     // end of message
	}
	outf.printf( "%s\n",bufp );

        //
        // display addresses and message size
        //
        if (inHeader  &&  (isHeader(bufp,"From")  ||  isHeader(bufp,"To"))) {
            OutputCR( OW_MAIL );
            OutputClrEol( OW_MAIL );
            if (firstFromTo) {
                if (msgSize > 0)
                    areas.mailPrintf1( OW_MAIL,"%s  (size=%ld)\n", bufp, msgSize );
                else
                    areas.mailPrintf1( OW_MAIL,"%s\n", bufp );
                firstFromTo = 0;
            }
            else
                areas.mailPrintf1( OW_MAIL,"  %s\n", bufp );
	    oldKbRead = -1;
	}

        if (bytesRead / 1024 != oldKbRead) {
            OutputCR( OW_MAIL );
            Output( OW_MAIL,"%s  (%05ldK of %05ldK)", sPercent, bytesRead/1024, pop3TotalSize/1024 );
            OutputClrEol( OW_MAIL );
	    oldKbRead = bytesRead;
	}
    }

    return ps_success;
}   // POP3_readmsg



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



static void pop3Cleanup( TSocket &socket, PopRetCode ok ) /*FOLD00*/
{
#ifdef TRACE
    printfT( "pop3CleanUp(.,%d)\n",ok );
#endif
    if (ok != ps_success  &&  ok != ps_socket)
	POP3_sendQuit(socket);
    
    if (ok == ps_socket) 
	perror("doPOP3: cleanUp");

    socket.close();
}   // pop3Cleanup



static int pop3Connect( TSocket &socket, const char *host, const char *userid, /*FOLD00*/
			const char *password, int port, int displayStatus )
//
//  opens the socket and returns number of messages in mailbox,
//  on error -1 is returned
//
{
    static int firstCall = 1;
    PopRetCode ok;
    int count;
    char timestamp[BUFSIZ];
    char *p1, *p2;
    int  tryAPOP = doAPOP;

#ifdef TRACE
    printfT( "pop3Connect(.,%s,%s,.,%d )\n", host,userid,port );
#endif

retryit:
    if (socket.open( host,"pop3","tcp",port ) < 0) {
	if (firstCall)
	    areas.mailPrintf1( OW_MAIL,"cannot connect to pop3 server %s\n", host);
	firstCall = 0;
	return -1;
    }

    if (firstCall)
	areas.mailPrintf1( OW_MAIL,"connected to pop3 server %s\n",host );
    firstCall = 0;
    
    ok = POP3_ok(timestamp, socket);
    if (ok != ps_success) {
	if (ok != ps_socket)
	    POP3_sendQuit(socket);
	socket.close();
	return -1;
    }
    
    //
    //  try to get authorized by APOP method 
    //  if only there was a hint of timestamp (aka Message-ID)
    //  in mailserver's response
    //
    //  hg:  I'm really not sure, what will happen, if APOP fails.  Will the connection
    //       be dropped, or will the POP3 server remain connected!?
    //
    p1 = strchr( timestamp, '<' );
    p2 = (p1 != NULL) ? strchr( p1, '>' ) : NULL;
    if (tryAPOP  &&  userid  &&  password  &&  p1  &&  p2  &&  p2 > p1) {
	ok = POP3_ApopAuth(userid, password, p1, p2-p1+1, socket);
	if (ok != ps_success) {
            areas.mailPrintf1( OW_MAIL,"APOP autentication failed, retrying without\n" );
            areas.mailPrintf1( OW_MAIL,"Disable APOP detection with -Y\n" );
	    sleep( 5 );
	    pop3Cleanup( socket,ok );
	    sleep( 5 );
	    tryAPOP = 0;
	    goto retryit;    // hah:  my first goto for several years (except for assembler of course)
	}
	areas.mailPrintf1( OW_MAIL,"APOP autentication succeeded\n" );
    }
    else {
	/* try to get authorized */
	ok = POP3_Auth(userid, password, socket);
	if (ok != ps_success) {
	    pop3Cleanup( socket,ok );
	    return -1;
	}
    }

    /* find out how many messages are waiting */
    ok = POP3_sendStat(&count, socket, displayStatus);
    if (ok != ps_success) {
	pop3Cleanup( socket,ok );
	return -1;
    }

    return count;
}   // pop3Connect



static int pop3WriteMail( TAreasMail &msgF, TFile &inF ) /*FOLD00*/
//
//  Copy the mail content from the temporary to the SOUP file
//  returns 1 on success, 0 otherwise
//
{
    long msgSize;
    long toRead, wasRead;
    char buf[4096];   // 4096 = good size for file i/o
    int  res;

    //
    //  Get message size.
    //
    msgSize = inF.tell();
    if (msgSize <= 0)
	return 1;	// Skip empty messages (is this condition successful?)

    msgF.msgStart( "Email","bn" );
    
    //
    //  Copy article body.
    //
    inF.seek(0L, SEEK_SET);
    res = 1;
    while (msgSize > 0) {
	toRead = ((size_t)msgSize < sizeof(buf)) ? msgSize : sizeof(buf);
	wasRead = inF.read(buf, toRead);
	if (wasRead != toRead) {
	    perror("read mail");
	    res = 0;
	    break;
	}
	assert( wasRead > 0 );
	if (msgF.msgWrite(buf, wasRead) != wasRead) {
	    perror("write mail");
	    res = 0;
	    break;
	}
	msgSize -= wasRead;
    }

    msgF.msgStop();
    return res;
}   // pop3WriteMail


    
/*********************************************************************
  function:      getMailStatus
  description:   retrieve status from the specified mail server
                 using Post Office Protocol 3.

  return value:  <  0 -> error
                 == 0 -> no messages
                 >  0 -> number of messages in mailbox
                 popclient.h
  calls:
 *********************************************************************/

int getMailStatus( const char *host, const char *userid, const char *password, int port ) /*FOLD00*/
{
    TSocket socket;
    int msgTotal;

#ifdef TRACE
    printfT( "getMailStatus(%s,%s,.)\n", host,userid );
#endif

    if (host == NULL) {
	areas.mailPrintf1( OW_MAIL,"no pop3 server defined\n" );
	return -1;
    }

    msgTotal = pop3Connect( socket,host,userid,password,port, 1 );
    return msgTotal;
}   // getMailStatus



/*********************************************************************
  function:      getMail
  description:   retrieve messages from the specified mail server
                 using Post Office Protocol 3.

  arguments:     
    options      fully-specified options (i.e. parsed, defaults invoked,
                 etc).

  return value:  exit code from the set of PS_.* constants defined in 
                 popclient.h
  calls:
 *********************************************************************/

int getMail( const char *host, const char *userid, const char *password, int port ) /*FOLD00*/
{
    PopRetCode ok;
    TSocket socket;
    int count;
    int percent;
    int msgTotal;
    int msgNumber;
    long msgSize;
    TFileTmp tmpF;

#ifdef TRACE
    printfT( "getMail(%s,%s,.)\n", host,userid );
#endif

    if (host == NULL) {
	areas.mailPrintf1( OW_MAIL,"no pop3 server defined\n" );
	return 0;
    }

    if ( !tmpF.open()) {
	areas.mailPrintf1( OW_MAIL,"cannot open temporary mail file\n" );
	return 0;
    }

    msgTotal = pop3Connect( socket,host,userid,password,port, 0 );
    if (msgTotal < 0)
	return 0;

    for (count = 1, msgNumber = 1;  count <= msgTotal;  ++count, ++msgNumber) {
        percent = (count * 100) / msgTotal;
        sprintfT( sPercent,"%d%%", percent );
        OutputCR( OW_MAIL );
        Output( OW_MAIL,"%s",sPercent );
        OutputClrEol( OW_MAIL );

        if (POP3_sendList(&msgSize,msgNumber,socket) != ps_success)
            msgSize = -1;
        
	ok = POP3_sendRetr(msgNumber,socket);
	if (ok != ps_success) {
	    pop3Cleanup( socket,ok );
	    return 0;
	}
	    
	ok = POP3_readmsg(socket, tmpF, msgSize);
	if (ok != ps_success) {
	    pop3Cleanup( socket,ok );
	    return 0;
	}
	if ( !pop3WriteMail( areas,tmpF )) {
	    pop3Cleanup( socket, ps_success );
	    areas.mailPrintf1( OW_MAIL,"cannot copy mail into SOUP file\n" );
	    return 0;
	}
	    
	if ( !readOnly) {
	    ok = POP3_sendDele(msgNumber,socket);
	    if (ok != ps_success) {
		pop3Cleanup( socket,ok );
		return 0;
	    }
	}
	if (forceMailDelete  &&  count < msgTotal) {
	    int res;
	    
	    POP3_sendQuit( socket );
	    socket.close();
	    res = pop3Connect( socket,host,userid,password,port, 0 );
	    if (res < 0) {
		areas.mailPrintf1( OW_MAIL,"cannot reconnect to pop3 server %s\n",host );
		return 0;
	    }
	    else if (res == 0)
		msgTotal = 0;    // msg vanished??
	    msgNumber = 0;
	}
    }
    OutputCR( OW_MAIL );
    OutputClrEol( OW_MAIL );
    POP3_sendQuit(socket);
    socket.close();
    return 1;
}   // getMail
