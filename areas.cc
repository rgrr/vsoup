//  $Id: areas.cc 1.21 1999/08/29 12:59:27 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  SOUP AREAS file management
//


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rgmts.hh>
#include "areas.hh"
#include "output.hh"



static TSemaphor msg;    // requested by msgStart, released by msgStop
static TSemaphor mail;   // requested by mailStart, released by mailStop



TAreas::TAreas( const char *areasName, const char *msgNamePattern )
{
    int i;
    
#ifdef TRACE_ALL
    printfT( "TAreas::TAreas(%s,%s)\n",areasName,msgNamePattern );
#endif

    msgCounter = 0;
    msgStarted = 0;
    
    TAreas::msgNamePattern = xstrdup(msgNamePattern);
    
    if ( !areasF.open(areasName,TFile::mwrite,TFile::obinary,1)) {
	perror(areasName);
	exit( EXIT_FAILURE );
    }

    for (i = 0;  i < AREAS_FIFOSIZE;  ++i)
	fifo[i].id = fifo[i].format = fifo[i].filename = NULL;
    fifoNext = 0;
    fifoMsgF = 0;
}   // TAreas::TAreas



TAreas::~TAreas()
{
#ifdef TRACE_ALL
    printfT( "TAreas::~TAreas()\n" );
#endif
    closeAll();

    ////  den ganzen Quatsch freigeben
}   // TAreas::~TAreas



void TAreas::closeAll( void )
{
#ifdef TRACE_ALL
    printfT( "TAreas::closeAll()\n" );
#endif
    //
    //  if message was not completed then delete it...
    //
    if (msgStarted) {
	msgStarted = 0;
	msgF.truncate( msgLenPos );
    }
    msgF.close();
    areasF.close(1);
////	delete msgNamePattern;
}   // TAreas::closeAll



int TAreas::msgWrite( const char *buf, int buflen )
{
#ifdef TRACE_ALL
    printfT( "TAreas::msgWrite(.,%d)\n",buflen );
#endif
    assert( msgStarted );
    return msgF.write( buf,buflen );
}   // TAreas::msgWrite



int TAreas::msgPrintf( const char *fmt, ... )
{
    va_list ap;
    int res;

    assert( msgStarted );
    va_start( ap, fmt );
    res = msgF.vprintf( fmt, ap );
    va_end( ap );
    return res;
}   // TAreas::msgPrintf



void TAreas::msgStart( const char *id, const char *format )
{
    int i, ptr;
    char fname[FILENAME_MAX];
    char name[FILENAME_MAX];
    
    msg.Request();
    msgStarted = 1;

#ifdef TRACE_ALL
    printfT( "TAreas::msgStart(%s,%s)\n", id,format );
#endif

    //
    //  is id/format in FIFO?
    //
    ptr = -1;
    if (fifo[fifoMsgF].id != NULL  &&  stricmp(fifo[fifoMsgF].id,id) == 0  &&
	stricmp(fifo[fifoMsgF].format,format) == 0) {
#ifdef DEBUG_ALL
	printfT( "TAreas::msgStart(): hit\n" );
#endif
	ptr = fifoMsgF;
    }
    else {
#ifdef DEBUG_ALL
	printfT( "TAreas::msgStart(): no hit\n" );
#endif
	msgF.close();
	for (i = 0;  i < AREAS_FIFOSIZE;  ++i) {   //// hier kann man auch intelligenter Suchen...
	    if (fifo[i].id != NULL  &&  stricmp(fifo[i].id,id) == 0  &&
		stricmp(fifo[i].format,format) == 0) {
		ptr = i;
		if ( !msgF.open(fifo[ptr].filename,TFile::mwrite,TFile::obinary)) {
		    perror( fifo[ptr].filename );
		    exit( EXIT_FAILURE );
		}

#ifdef DEBUG_ALL
		printfT( "TAreas::msgStart(): reopening %s\n",fifo[ptr].filename );
#endif
		break;
	    }
	}
    }

    //
    //  wenn Id/Format nicht in FIFO drin, dann neue Datei anlegen und
    //  ein Element aus dem FIFO entfernen (bei fifoNext)
    //
    if (ptr < 0) {
	ptr = fifoNext;
	if (++fifoNext >= AREAS_FIFOSIZE)
	    fifoNext = 0;
	xstrdup( &(fifo[ptr].id),id );
	xstrdup( &(fifo[ptr].format),format );

	//
	//  open new message file & skip write-protected files
	//
	for (;;) {
	    ++msgCounter;
	    sprintfT( name, msgNamePattern,msgCounter );
	    strcpy( fname,name );
	    if (strcmp(format,"ic") == 0)
		strcat( fname,".IDX" );
	    else
		strcat( fname,".MSG" );
#ifdef DEBUG_ALL
	    printfT( "TAreas::msgStart():  creating: %s\n", fname );
#endif
	    if (msgF.open(fname,TFile::mwrite,TFile::obinary,1))
		break;
#ifndef HANDLEERR
	    if (errno != EACCES) {         // catch 'not enough file handles'
		perror( fname );
		exit( EXIT_FAILURE );
	    }
#endif
	}
#ifdef DEBUG_ALL
	printfT( "TAreas::msgStart(): ok %s\n", fname );
#endif
	xstrdup( &(fifo[ptr].filename),fname );
	areasF.printf( "%s\t%s\t%s\n", name, id, format );
	areasF.flush();
    }
    fifoMsgF = ptr;

    msgLenPos = -1;
    if (*format == 'b'  ||  *format == 'B') {
	static char buf[4] = {0,0,0,0};
	msgLenPos = msgF.tell();
	msgF.write( &buf, sizeof(buf) );
#ifdef DEBUG_ALL
	printfT( "TAreas::msgStart() msgLenPos = %ld\n", msgLenPos );
#endif
    }
}   // TAreas::msgStart



void TAreas::msgStop( void )
{
#ifdef TRACE_ALL
    printfT( "TAreas::msgStop()\n" );
#endif
    if (msgLenPos >= 0) {
	char buf[4];
	long msgLen = msgF.tell() - msgLenPos - 4;
	buf[0] = (char)(msgLen >> 24);
	buf[1] = (char)(msgLen >> 16);
	buf[2] = (char)(msgLen >>  8);
	buf[3] = (char)(msgLen >>  0);
	msgF.seek( msgLenPos,SEEK_SET );
	msgF.write( &buf,sizeof(buf) );
	msgF.seek( 0,SEEK_END );
#ifdef DEBUG_ALL
	printfT( "TAreas::msgStop():  msgLen=%ld\n",msgLen );
#endif
    }
	
    msgStarted = 0;
    msgF.flush();
    msg.Release();
}   // TAreas::msgStop



//--------------------------------------------------------------------------------



TAreasMail::TAreasMail( const char *areasName, const char *msgNamePattern ): TAreas( areasName,msgNamePattern )
{
#ifdef TRACE_ALL
    printfT( "TAreasMail::TAreasMail(%s,%s)\n",areasName,msgNamePattern );
#endif
    mailName = xstrdup("STSMAIL");
    mailStarted = 0;
    mailForced = 0;
    mailExcept = 0;
#ifdef TRACE_ALL
    printfT( "TAreasMail::TAreasMail: finished\n" );
#endif
}   // TAreasMail::TAreasMail



TAreasMail::~TAreasMail()
{
#ifdef TRACE_ALL
    printfT( "TAreasMail::~TAreasMail()\n" );
#endif
////    assert( !mailStarted );  thread-killerei
}   // TAreasMail::~TAreasMail



void TAreasMail::mailOpen( const char *title )
{
    char fname[FILENAME_MAX];
    time_t now;
    char dateBuf[200];
    struct tm *nowtm;

    assert( !mailStarted );
    assert( !mailF.isOpen() );
    
    sprintfT( fname,"%s.MSG",mailName );
    if ( !mailF.open(fname,TFile::mwrite,TFile::obinary,1)) {
	perror(fname);
	exit( EXIT_FAILURE );
    }
    now = time(NULL);
    nowtm = localtime( &now );
    strftime( dateBuf, sizeof(dateBuf), "%a, %d %b %Y %H:%M %Z",nowtm );
    mailF.printf( "From POPmail %s\n",dateBuf );
    mailF.printf( "To: VSoupUser\n" );
    mailF.printf( "From: VSoup\n" );
    mailF.printf( "Subject: VSoup status report: %s\n", title );
    mailF.printf( "Date: %s\n\n", dateBuf );
}   // TAreasMail::mailOpen



int TAreasMail::mailPrintf( const char *fmt, ... )
{
    va_list ap;
    int res;

    assert( mailF.isOpen() );

    if ( !mailExcept)
	mpSema.Request();

    if (mailFirstLine) {
	time_t now = time(NULL);
	mailF.printf( "\n---------- %s", ctime(&now) );
	mailFirstLine = 0;
    }
    va_start( ap, fmt );
    res = mailF.vprintf( fmt, ap );
    va_end( ap );

    if ( !mailExcept)
	mpSema.Release();
    return res;
}   // TAreasMail::mailPrintf



int TAreasMail::mailPrintf1( int echoWin, const char *fmt, ... )
//
//  prints a single line to the mail file without any header...
//
{
    va_list ap;
    int res;

    assert( mailF.isOpen() );

    if ( !mailExcept)
	mail.Request();

////    assert( !mailStarted );   // possible during exceptions
    mailStarted = 1;

    switch (echoWin) {
        case OW_NEWS:
            mailF.printf( "news: " );
            break;
        case OW_MAIL:
            mailF.printf( "mail: " );
            break;
        case OW_XMIT:
            mailF.printf( "xmit: " );
            break;
        default:
            mailF.printf( "----: " );
            break;
    }
    
    va_start( ap, fmt );
    res = mailF.vprintf( fmt, ap );
    if (echoWin != 0)
        OutputV( echoWin, fmt, ap );
    va_end( ap );
    mailF.flush();

    mailStarted = 0;
    if ( !mailExcept)
	mail.Release();
    return res;
}   // TAreasMail::mailPrintf1



void TAreasMail::mailStart( void )
{
#ifdef TRACE_ALL
    printfT( "TAreasMail::mailStart()\n" );
#endif
    if ( !mailExcept)
	mail.Request();
    mailStarted   = 1;
    mailFirstLine = 1;
//// new line + datum in Mail ausgeben / mail ”ffnen...
}   // TAreasMail::mailStart



void TAreasMail::mailStop( void )
{
#ifdef TRACE_ALL
    printfT( "TAreasMail::mailStop()\n" );
#endif
    if ( !mailFirstLine)
	mailPrintf( "-----------------------------------\n\n" );
    mailF.flush();
    mailStarted = 0;
    if ( !mailExcept)
	mail.Release();
}   // TAreasMail::mailStop



void TAreasMail::closeAll( void )
{
#ifdef TRACE_ALL
    printfT( "TAreasMail::closeAll()\n" );
#endif
    if (mailF.isOpen()) {
	mailF.flush();                       // sollte eigentlich nicht notwendig sein !?
	if (mailForced) {
	    mailF.close(1);
	    areasF.printf( "%s\t%s\t%s\n",mailName,"Email","mn" );
	    areasF.flush();
	}
	else
	    mailF.remove();
////    delete mailName;
    }
    TAreas::closeAll();
}   // TAreasMail::closeAll
