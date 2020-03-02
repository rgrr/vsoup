//  $Id: reply.cc 1.20 1999/08/29 13:14:50 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  Send reply packet.
//


#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <rgfile.hh>
#include <rgmts.hh>
#include <rgsocket.hh>

#include "global.hh"
#include "reply.hh"
#include "output.hh"
#include "util.hh"
#include "nntpcl.hh"
#include "smtp.hh"

static TSocket smtpSock;
static TNntp nntpReply[MAXNNTPXMTCNT];
static char *mailer;
static char *poster;

static int nntpConnected[MAXNNTPXMTCNT];   // -1 failed, 0 try to connect, 1 connected
static int smtpConnected;



//--------------------------------------------------------------------------------



static void _connectServer( void *no )
{
    int srvno = (int)no;

    if (srvno < 0) {
	if (smtpInfo.host == NULL  ||  *smtpInfo.host == '\0')
	    smtpConnected = -1;
	else {
	    if (smtpConnect(smtpSock)) {
		areas.mailPrintf1( OW_XMIT,"connected to smtp gateway %s\n",
				   smtpInfo.host );
		smtpConnected = 1;
	    }
	    else {
		areas.mailPrintf1( OW_XMIT,"cannot connect to smtp gateway %s\n",
				   smtpInfo.host );
		smtpConnected = -1;
	    }
	}
    }
    else {
	if (nntpInfo[srvno].host == NULL  ||  *nntpInfo[srvno].host == '\0')
	    nntpConnected[srvno] = -1;
	else {
	    if (nntpReply[srvno].open( nntpInfo[srvno].host, nntpInfo[srvno].user,
				       nntpInfo[srvno].passwd, nntpInfo[srvno].port, 1) == TNntp::ok) {
		areas.mailPrintf1( OW_XMIT,"connected to news server %s (post)\n",
				   nntpInfo[srvno].host );
		nntpConnected[srvno] = 1;
	    }
	    else {
		areas.mailPrintf1( OW_XMIT,"cannot connect to news server %s (post):\n        %s\n",
				   (nntpInfo[srvno].host != NULL) ? nntpInfo[srvno].host : "-unknown-",
				   nntpReply[srvno].getLastErrMsg() );
		nntpConnected[srvno] = -1;
	    }
	}
    }
}   // _connectServer



static void connectServer( void )
//
//  _initiate_ connect to destination servers, i.e. do not wait until connected
//
{
    int i;

    smtpConnected = 0;
    BEGINTHREAD( _connectServer, (void *)-1 );

    nntpConnected[0] = -1;
    for (i = 0;  i < nntpXmtCnt;  ++i) {
	nntpConnected[i] = 0;
	BEGINTHREAD( _connectServer, (void *)i );
    }
}   // connectServer



static int waitConnect( int srvno )
//
//  wait until connection established or failed.
//  srvno==-1  ->  smtp, otherwise nntp[srvno]
//  returns 1 if connection ok, 0 if failed
//
{
    int res = 0;

    for (;;) {
	if (srvno == -1)
	    res = smtpConnected;
	else
	    res = nntpConnected[srvno];

#ifdef DEBUG
	printfT( "%d,%d  ",srvno,res );
#endif

	if (res == 0)
	    sleep( 1 );
	else
	    break;
    }
    return (res > 0);
}   // waitConnect



static void _closeServer( void *no )
{
    int srvno = (int)no;

#ifdef DEBUG
    printfT( "_closeServer(%d)\n", srvno );
#endif

    if (srvno < 0) {
	if (smtpConnected > 0) {
	    smtpClose( smtpSock );
	    smtpConnected = -1;
	}
    }
    else {
	if (nntpConnected[srvno] > 0) {
	    nntpReply[srvno].close( 1 );
	    nntpConnected[srvno] = -1;
	}
    }

#ifdef DEBUG
    printfT( "_closeServer(%d) finished\n", srvno );
#endif
}   // _closeServer



static void closeServer( void )
//
//  close server connection and wait until finished (max 5s)
//
{
    int i;
    int stillConnected;
    int timeout;
    
#if DEBUG
    printfT( "closeServer()\n" );
#endif
    BEGINTHREAD( _closeServer, (void *)-1 );
    for (i = 0;  i < nntpXmtCnt;  ++i)
	BEGINTHREAD( _closeServer, (void *)i );
    
    for (timeout = 0;  timeout < 5;  ++timeout) {
	stillConnected = 0;
	if (smtpConnected > 0)
	    stillConnected = 1;
	for (i = 0;  i < nntpXmtCnt;  ++i) {
	    if (nntpConnected[i] > 0)
		stillConnected = 1;
	}
	if (stillConnected)
	    sleep( 1 );
	else
	    break;
    }
}   // closeServer



//--------------------------------------------------------------------------------



static int sendPipe( TFile &fd, size_t bytes, const char *agent )
//
//  Pipe a message to the specified delivery agent.
//
{
    FILE *pfd;
    char buff[4096];
    int cnt;

    /* Open pipe to agent */
    if ((pfd = popenT(agent, "wb")) == NULL) {
	areas.mailPrintf1( OW_XMIT,"cannot open reply pipe %s\n", agent );
	while (bytes > 0) {
	    cnt = (bytes >= sizeof(buff)) ? sizeof(buff) : bytes;
	    fd.read( buff,cnt );
	    bytes -= cnt;
	}
	return 0;
    }

    /* Send message to pipe */
    while (bytes > 0) {
	cnt = (bytes >= sizeof(buff)) ? sizeof(buff) : bytes;
	if (fd.read(buff,cnt) != cnt) {
	    areas.mailPrintf1( OW_XMIT,"ill reply file: %s\n", fd.getName() );
	    return 0;
	}
	fwriteT( buff,1,cnt,pfd );
	bytes -= cnt;
    }

    pcloseT(pfd);
    return 1;
}   // sendPipe



static int sendMail( TFile &inf, size_t bytes )
{
    int res = 1;
    if (mailer) {
	const char *to = getHeader(inf, "To");
	areas.mailPrintf1( OW_XMIT,"mailing to %s\n", to );
	delete to;

	/* Pipe message to delivery agent */
	res = sendPipe(inf, bytes, mailer);
    }
    else {
        if (waitConnect(-1)) {
            int r = smtpMail(smtpSock, inf, bytes);
            if (r != 1) {
                areas.mailPrintf1( OW_XMIT,"cannot deliver mail\n" );
                areas.forceMail();
                if (r == 2) {
                    areas.mailPrintf1( OW_XMIT,"reconnecting...\n" );
                    _closeServer( (void *)-1 );
                    sleep( 1 );
                    smtpConnected = 0;
                    BEGINTHREAD( _connectServer, (void *)-1 );
                }
                res = 0;
            }
	}
	else
	    res = 0;
    }
    return res;
}   // sendMail



static int sendNews( TFile &inf, size_t bytes, int doNewsSend )
//
//  returns  1 if everything ok,
//           0 if this article failed,
////          -1 if there was a fatal problem (with the connection)
//
{
    int res = -1;
    const char *grp;
    long offset;

#ifdef DEBUG
    printfT( "sendNews( .,%ld,%d )\n", bytes,doNewsSend );
#endif

    waitConnect( 0 );

    grp = getHeader( inf, "Newsgroups" );
    areas.mailPrintf1( OW_XMIT,"posting article to %s\n", grp );
    delete grp;

    offset = inf.tell();

    if (poster) {
	/* Pipe message to delivery agent */
	res = sendPipe(inf, bytes, poster);
    } else {
	int srvno;

	for (srvno = 0;  srvno < nntpXmtCnt;  ++srvno) {
	    TNntp::Res nres;
	    
	    inf.seek(offset, SEEK_SET);
	    
	    res = -1;
	    if (waitConnect(srvno)) {
		if (doNewsSend) {
		    nres = nntpReply[srvno].postArticle( inf,bytes );
		}
		else {
		    const char *id = getHeader( inf, "Message-ID" );
		    nres = nntpReply[srvno].ihaveArticle( inf,bytes,id );
		    delete id;
		}
		
		if (nres == TNntp::nok) {
		    res = 0;
		    nntpConnected[srvno] = -1;
		}
		else if (nres == TNntp::nok_goon)
		    res = 0;
		else if (nres == TNntp::ok) {
		    res = 1;
		    break;
		}
	    }
		
	    if (res <= 0)
		areas.mailPrintf1( OW_XMIT,"    cannot %s article to %s:\n        %s\n",
				   doNewsSend ? "post" : "push",
				   nntpInfo[srvno].host,
				   (res == -1) ? "not connected" : nntpReply[srvno].getLastErrMsg());
	}
	if (res > 0)
	    areas.mailPrintf1( OW_XMIT,"    %s to %s\n",
			       doNewsSend ? "posted" : "pushed",
			       nntpInfo[srvno].host);
    }
#ifdef DEBUG
    printfT( "sendNews() = %d\n",res );
#endif
    inf.seek(offset+bytes, SEEK_SET);

    if (res == 1)
	return 1;
    return 0;
}   // sendNews



static int sendMailu (const char *fn)
//
//  Process a mail reply file, usenet type (is that one really correct???)
//
{
    char buf[BUFSIZ];
    TFile fd;
    int bytes;
    int res = 1;

    //
    //  Open the reply file
    //  problem here is non-fatal!
    //
    if ( !fd.open(fn,TFile::mread,TFile::obinary)) {
	areas.mailPrintf1( OW_XMIT,"cannot open file %s\n", fn );
	return 1;
    }

    /* Read through it */
    while (fd.fgets(buf,sizeof(buf),1)) {
        if (strncmp (buf, "#! rnews ", 9)  &&  strncmp(buf, "#! rmail ",9)) {
	    areas.mailPrintf1( OW_XMIT,"malformed reply file\n" );
	    res = 0;
	    break;
	}

	/* Get byte count */
	sscanfT(buf+9, "%d", &bytes);

	if ( !sendMail(fd, bytes)) {
	    res = 0;
	    break;
	}
    }
    fd.close();
    return res;
}   // sendMailu



static int sendNewsu( const char *fn, int doNewsSend )
//
//  Process a news reply file, usenet type
//
{
    char buf[BUFSIZ];
    TFile fd;
    int bytes;
    int res = 1;

    if (nntpXmtCnt == 0) {
	areas.mailPrintf1( OW_XMIT,"cannot transmit news:  no news server defined\n" );
	return 0;
    }
    
    //
    //  Open the reply file
    //  problem here is non-fatal!
    //
    if ( !fd.open(fn,TFile::mread,TFile::obinary)) {
	areas.mailPrintf1( OW_XMIT,"cannot open file %s\n", fn );
	return 1;
    }

    /* Read through it */
    while (fd.fgets(buf,sizeof(buf),1)) {
	if (strncmp (buf, "#! rnews ", 9)) {
	    areas.mailPrintf1( OW_XMIT,"malformed reply file\n");
	    res = 0;
	    break;
	}

	sscanfT(buf+9, "%d", &bytes);
	if ( !sendNews(fd, bytes, doNewsSend))
	    res = 0;
    }
    fd.close();
    return res;
}   // sendNewsu



static int sendMailb (const char *fn)
//
//  Process a mail reply file, binary type
//  The binary type is handled transparent, i.e. CRLF are two characters!
//
{
    unsigned char count[4];
    TFile fd;
    int bytes;
    int res = 1;

    //
    //  Open the reply file
    //  problem here is non-fatal!
    //
    if ( !fd.open(fn,TFile::mread,TFile::obinary)) {
	areas.mailPrintf1( OW_XMIT,"cannot open file %s\n", fn );
	return 1;
    }

    /* Read through it */
    while (fd.read(count,4) == 4) {
	/* Get byte count */
	bytes = ((count[0]*256 + count[1])*256 + count[2])*256 + count[3];
	if ( !sendMail(fd, bytes)) {
	    res = 0;
	    break;
	}
    }

    fd.close();
    return res;
}   // sendMailb



static int sendNewsb( const char *fn, int doNewsSend )
//
//  Process a news reply file, binary type
//  The binary type is handled transparent, i.e. CRLF are two characters!
//
{
    unsigned char count[4];
    TFile fd;
    int bytes;
    int res = 1;

    if (nntpXmtCnt == 0) {
	areas.mailPrintf1( OW_XMIT,"cannot transmit news:  no news server defined\n" );
	return 0;
    }
    
    //
    //  Open the reply file
    //  problem here is non-fatal!
    //
    if ( !fd.open(fn,TFile::mread,TFile::obinary)) {
	areas.mailPrintf1( OW_XMIT,"cannot open file %s\n", fn );
	return 1;
    }

    /* Read through it */
    while (fd.read(count, 4) == 4) {
	bytes = ((count[0]*256 + count[1])*256 + count[2])*256 + count[3];
	if ( !sendNews(fd, bytes, doNewsSend))
	    res = 0;
    }
    fd.close();
    return res;
}   // sendNewsb



//--------------------------------------------------------------------------------



int sendReply( int doNewsSend )
//
//  Process a reply packet.
//
{
    TFile rep_fd;
    char buf[BUFSIZ];
    char fname[FILENAME_MAX], kind[FILENAME_MAX], type[FILENAME_MAX];
    int mailError = 0;
    int nntpError = 0;

    //
    //  Get MAILER/POSTER from the environment.
    //  But:  only if there was no hostname specified!
    //
    if (smtpInfo.host == NULL) {
	mailer = getenv("MAILER");
	if (mailer)
	    areas.mailPrintf1( OW_XMIT,"environmment variable MAILER has the value '%s'\n", mailer );
    }
    if (nntpInfo[0].host == NULL) {
	poster = getenv("POSTER");
	if (poster)
	    areas.mailPrintf1( OW_XMIT,"environmment variable POSTER has the value '%s'\n", poster );
    }
    
    //
    //  Open the packet
    //  if none exists -> non-fatal
    //
    if ( !rep_fd.open(FN_REPLIES,TFile::mread,TFile::otext)) {
	areas.mailPrintf1( OW_XMIT,"cannot open file %s (non-fatal)\n", FN_REPLIES);
	return 1;
    }

    //
    //  connect to destination servers
    //
    connectServer();

    /* Look through lines in REPLIES file */
    while (rep_fd.fgets(buf, sizeof(buf), 1)) {
	if (sscanfT(buf, "%s %s %s", fname, kind, type) != 3) {
	    areas.mailPrintf1( OW_XMIT,"malformed REPLIES line: %s\n", buf);
	    areas.forceMail();            // indicates corrupt replies file
	    return 0;
	}

	//
	//  Check reply type
	//
	if (type[0] != 'u' && type[0] != 'b' && type[0] != 'B') {
	    areas.mailPrintf1( OW_XMIT,"reply type %c not supported: %s\n", type[0], buf);
	    areas.forceMail();            // indicates corrupt replies file
	    continue;
	}
	if (type[0] == 'u') {
	    if (strcmp(kind,"mail") != 0  &&  strcmp(kind,"news") != 0) {
		areas.mailPrintf1( OW_XMIT,"bad reply kind %s in: %s\n", kind, buf);
		areas.forceMail();        // indicates corrupt replies file
		continue;
	    }
	}

	/* Make file name */
	strcat(fname, ".MSG");

	/*
	**  Wenn Datei nicht existiert heiát das, daá sie schon
	**  versendet wurde (und dies sozusagen ein RETRY ist)
	*/

	/* Process it */
	switch (type[0]) {
	    case 'u':
		if (strcmp(kind, "mail") == 0) {
		    if ( !sendMailu(fname)) {
			mailError = 1;
			continue;
		    }
		}
		else if (strcmp(kind, "news") == 0) {
		    if ( !sendNewsu(fname,doNewsSend)) {
			nntpError = 1;
			continue;
		    }
		}
		break;
	    case 'b':
		if ( !sendMailb(fname)) {
		    mailError = 1;
		    continue;
		}
		break;
	    case 'B':
		if ( !sendNewsb(fname,doNewsSend)) {
		    nntpError = 1;
		    continue;
		}
		break;
	}

	/* Delete it */
	if ( !readOnly)
	    removeT(fname);
    }

    closeServer();

    //
    //  remove REPLIES only, if all files were removed
    //
    if ( !readOnly  &&  !mailError  &&  !nntpError)
	rep_fd.remove();
    else
	rep_fd.close();

    return !mailError  &&  !nntpError;
}   // sendReply
