//  $Id: news.cc 1.50 1999/08/29 13:07:15 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  Get news from NNTP server.
//
//  rg270596:
//  - multithreading support for OS/2
//
//  Und nun mal ein paar Erfahrungen mit multithreading (in diesem Zusammenhang):
//  -----------------------------------------------------------------------------
//
//  Probleme mit CHANGI:
//  - die ursprÅngliche CHANGI Version "spann" bei vielfachem Zugriff
//    Update nach changi09m brachte erhebliche Erleichterung
//  - nntp-NEXT tut nicht, wenn gleichzeitig ein Artikel gelesen wird,
//    oder wenn mehrere NEXTs unterwegs sind (kînnte CHANGI-Problem sein)
//    120696:  war eher meine Blîdheit und hatte u.U. was mit den Signalen zu tun...
//             Wahrscheinlich braucht der NEXT recht lange und der entsprechende
//             gets() wurde dann mit hoher Wahrscheinlichkeit durch ein Signal
//             unterbrochen...
//
//  Probleme mit EMX-GCC09a:
//  - das signal-handling scheint fragwÅrdig (signal mu· aber abgefangen werden...):
//    - nextchar in socket (wird durch recv/read gemacht) kommt u.U. mit EINTR
//      zurÅck - doch wie setze ich dann wieder auf???
//    - _beginthread kommt u.U. mit EINVAL zurÅck, was jedoch ebenso auf einen
//      unterbrochenen Aufruf schlie·en lÑ·t (d.h. Fehlerauswertung nicht vollstÑndig)
//    --> Signale nicht dazu verwenden, um die Beendigung eines Threads anzuzeigen,
//        sondern nur in absoluten NotfÑllen!!!
//  - einmal (?) hatte ich im (durch Semaphor geschÅtzten) StdOut einen grî·eren
//    Block doppelt.  Das Programm hat den Block unmîglich (?) produzieren kînnen,
//    also kommt nur EMX-GCC in Frage bzw. das OS
//  - new/delete kînnen nicht selbst definiert werden
//  - wie komme ich bitte an _threadid ? (stddef.h war nicht fÅr C++)
//  - unlink steht nicht in stdio.h, sondern unistd.h
//  - tmpfile() / tempnam() durch Semaphor geschÅtzt ??
//  --> hÑtte ich Zugriff, wÅrde ich sofort nach 09b updaten!!!
//
//  Hausgemachte Probleme:
//  - ein mehrfacher Request von einem MutexSemaphor in EINEM Thread hÑlt diesen
//    NICHT an.  Nur ein anderer Thread kann das Semaphor nicht mehr anfordern...
//  - Zustandsmaschine war durch 'mode reader' nicht mehr korrekt (es wurde schon ein
//    'waiting' angezeigt, obwohl noch 'init' war...)
//    - in nntpMtGetFinished wurde der Zustand zweimal abgefragt und dann noch in der
//      Reihenfolge 'finished'?, 'running'?.  Dieser öbergang wird aber in einem Thread
//      gemacht -> Thread war u.U. noch nicht 'finished', aber auch nicht mehr 'running'.
//      Dies ergibt ein leicht inkonsistentes Bild der ZustÑnde!
//  - wird ein ZÑhler im Thread hochgezÑhlt und mu· hinterher ausgewertet
//    werden, so empfiehlt sich mindestens ein Semaphor (vielleicht auch noch
//    volatile) (bytesRcvd)
//  - die Threads mÅssen auch einen Signal-Handler fÅr z.B. SIGPIPE haben, sonst
//    gibt es bei Abbruch u.U. einen doppelten Fehler! (das kommt daher, wie ein
//    Programm abgebrochen wird)
//  - ein Event-Semaphor will auch zurÅckgesetzt werden!  Die 'Kinder' laufen sonst
//    echt Gefahr zu verhungern...
//  - stream-I/O mu· konsequent durch MuxSema geschÅtzt werden (ein bi·chen Disziplin
//    bitte)
//  - regexp hat statische Variablen
//  - um Klassen, die was mit Listen oder so zu tun haben, am besten auch ein
//    individuelles Semaphor legen
//  - stimmt der makefile nicht, und es wird ein Datentyp geÑndert, so kommt es
//    klarerweise zu seltsamen Effekten (die Objekte werden nicht neu angelegt, etc.)
//  - beachte Zuweisung eines 'char' von 0xff (== -1) an einen int!!!
//


#include <assert.h>
#include <process.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rgfile.hh>
#include <rgmts.hh>
#include <rgsocket.hh>

#include "areas.hh"
#include "global.hh"
#include "kill.hh"
#include "news.hh"
#include "newsrc.hh"
#include "nntp.hh"
#include "nntpcl.hh"
#include "output.hh"



static TSemaphor   xhdrSema;
static TSemaphor   syncTransSema;
static TEvSemaphor threadFinito;
static TEvSemaphor disconnectDone;

static TProtCounter artsRcvd;         // articles received...
static TProtCounter artsKilled;       // articles killed...
static TProtCounter artsTot;          // total number of articles (just for displaying)
static TProtCounter artsGotten;       // estimated number of gotten articles
static TProtCounter activeRoutines;   // used by mtGetGroup()

static TKillFile killF;               // kill file handling

static volatile int doingProcessSendme;        // currently doing processSendme
static volatile int stopOperation = 0;         // we are in the finished loop - stop all unimportant ops (getXhdr)
static volatile int abortOperation = 0;        // absolutely finish

static long catchupNumKeep;

//
//  thread states (init must be 0)
//  starting is for debugging and not absolutely required...
////  (are static vars initialized to zero ?)
//
enum NntpStates { init,connecting,failed,waiting,starting,
		  running,runningspecial,aborted };

//
//  these are the thread connections to the news server
//
static TNntp nntp[ MAXNNTPTHREADS ];             // no pointer (because of imlicit destructor)
static volatile NntpStates nntpS[ MAXNNTPTHREADS ] = {init};
static volatile int nntpSyncCall[ MAXNNTPTHREADS ];



//--------------------------------------------------------------------------------
//
//  utility functions
//



#if defined(TRACE)  ||  defined(TRACE_ALL)  ||  defined(DEBUG)  ||  defined(DEBUG_ALL)
static void printThreadState( const char *pre, int maxNo=maxNntpThreads )
{
    int i;
    char b[1000];

    assert( maxNo <= maxNntpThreads );

    sprintfT( b,"%s: ",pre );
    for (i = 0;  i < maxNo;  i++) {
	switch (nntpS[i]) {
	case init:
	    strcat(b,"[i]");
	    break;
	case connecting:
	    strcat(b,"[c]");
	    break;
	case starting:
	    strcat(b,"[s]");
	    break;
	case waiting:
	    strcat(b,"[w]");
	    break;
	case running:
	    strcat(b,"[r]");
	    break;
	case runningspecial:
	    strcat(b,"[R]");
	    if (maxNo < maxNntpThreads)
		++maxNo;
	    break;
	case failed:
	    strcat(b,"[E]");
	    if (maxNo < maxNntpThreads)
		++maxNo;
	    break;
	case aborted:
	    strcat(b,"[A]");
	    if (maxNo < maxNntpThreads)
		++maxNo;
	    break;
	}
    }
    Output( OW_NEWS,"%s\n",b );
}
#endif



static long killArticleQ( const char *groupName, const char *headerLine )
//
//  this is a hook function for TNntp...
//  Attention:  groupName/headerLine are NULL, if killQHook is called the first
//              time for the current header!
//
{
    return killF.matchLine( groupName, headerLine );
}   // killArticleQ



static void processXref( const char *s )
//
//  Process an Xref line.
//  format: 'Xref: '<host-name> <grp-name[: ]grp-num>(\b<grp-name[: ]grp-num>)*
//  - s points behind 'Xref: '
//  - \b may be blank or \t
//  
//  rg260596:  the new version works with sscanf (before strtok).  Hopefully this version
//             is ok for multithreading
//
//  hook function for TNntp...
//
//
{
    const char *p;
    char name[FILENAME_MAX];
    int num, cnt;

#ifdef DEBUG_ALL
    printfT( "XREF: '%s'\n",s );
#endif

    //
    //  Skip the host field
    //
    p = strpbrk( s," \t" );
    if (p == NULL)
	return;

    //
    //  Look through the rest of the fields
    //  (note:  the %n does not count in the sscanf-result)
    //
    while (sscanfT(p,"%*[ \t]%[^ \t:]%*[ \t:]%d%n",name,&num,&cnt) == 2) {
#ifdef DEBUG_ALL
	printfT( "xref: '%s' %d\n",name,num );
#endif
	newsrc.artMarkRead( name,num );
	p += cnt;
    }
}   // processXref



static int writeArticle( TAreasMail &msgF, TFile &inF, const char *groupName )
//
//  Copy article from temporary file to TAreas-msgfile.
//  Current file pointer points to end of article
//  Return TRUE if article was copied (successfully).
//
{
    long artSize;
    long toRead, wasRead;
    char buf[4096];   // 4096 = good size for file i/o
    int  res;

    //
    //  Get article size.
    //
    artSize = inF.tell();
#ifdef DEBUG_ALL
    if (artSize <= 0)
	printfT( "writeArticle(): ftellT: %p,%ld,%ld\n",&inF,artSize,inF.tell() );
#endif
    if (artSize <= 0  ||  doAbortProgram)
	return 0;	// Skip empty articles

    msgF.msgStart( groupName,"Bn" );
    
    //
    //  Copy article body.
    //
    inF.seek(0L, SEEK_SET);
    res = 1;
    while (artSize > 0) {
	toRead = ((size_t)artSize < sizeof(buf)) ? artSize : sizeof(buf);
	wasRead = inF.read(buf, toRead);
	if (wasRead != toRead) {
	    perror("read article");
#ifdef DEBUG_ALL
	    printfT( "writeArticle: read article: %p,%lu,%lu,%p,%lu,%lu\n",buf,toRead,wasRead,&inF,inF.tell(),artSize );
#endif
	    res = 0;
	    break;
	}
	assert( wasRead > 0 );
	if (msgF.msgWrite(buf, wasRead) != wasRead) {
	    perror("write article");
	    res = 0;
	    break;
	}
	artSize -= wasRead;
    }

    msgF.msgStop();
    return res;
}   // writeArticle



static void mtThroughputInfo( void *arg )
{
    int sleepMs = (int)arg;
    int cnt = 0;
    unsigned char rotate[] = {196,192,179,218,196,191,179,217};
    long arts, ks, got, tot, bytes, oldBytes;
    char msg[80];

#if defined(DEBUG)  ||  defined(DEBUG_ALL)
    return;
#endif
#if defined(OS2)  &&  defined(__MT__)
    DosSetPriority(PRTYS_THREAD, PRTYC_NOCHANGE,5, 0);
#endif

    oldBytes = 0;
    for (;;) {
	bytes = TSocket::getTotalBytesRcvd();
	if (bytes != oldBytes)
	    ++cnt;
	else
	    cnt ^= 1;
	oldBytes = bytes;

	ks   = bytes / 1000;
	arts = artsRcvd;
	got  = artsGotten;
	tot  = artsTot;
#ifdef DEBUG
	areas.mailPrintf1( 0,"%%: %ld %ld %ld %ld%%\n", arts, got, tot,
			   (100*got) / ((tot != 0) ? tot : 1) );
	if (got > tot)
	    printfT( "\n####\n" );
#endif
	if (tot == 0)
	    sprintfT( msg,"(%05ldk)", ks );
	else {
	    if (doingProcessSendme)
		sprintfT( msg,"%6ld (%ld: %05ldk)", tot, arts, ks );
	    else
		sprintfT( msg,"%3d%% (%ld: %05ldk)",(int)((100*got) / tot), arts, ks );
        }
        OutputCR( OW_NEWS );
        Output( OW_NEWS,"%c  %s",rotate[cnt % sizeof(rotate)],msg );
        OutputClrEol( OW_NEWS );
        OutputCR( OW_NEWS );
	_sleep2( sleepMs );
        if (abortOperation  ||  doAbortProgram)
            break;
    }
    OutputCR( OW_NEWS );
    OutputClrEol( OW_NEWS );
}   // mtThroughputInfo



static int enoughRcvdQ()
//
//  returns '1' if we have read enough
//
{
    static int msgDisplayed = 0;
    int res = 0;

    if (maxBytes > 0  &&  TSocket::getTotalBytesRcvd() >= maxBytes) {
	blockThread();
	if ( !msgDisplayed) {
	    msgDisplayed = 1;
	    unblockThread();
	    areas.mailPrintf1( OW_NEWS,"ok, we've read enough...\n" );
	}
	else
	    unblockThread();
	res = 1;
    }
    return res;
}   // enoughRcvdQ



//--------------------------------------------------------------------------------



#if defined(__MT__)
static void signalHandlerThread( int signo )
{
#ifndef NDEBUG
    printfT( "\nthread received signal %d\n",signo );
#endif
    signal( signo, SIG_DFL );
    _endthread();
}   // signalHandlerThread
#endif



static void mtInitSignals( void )
//
//  very important to call this function during init of a THREAD ////???
//
{
#ifdef TRACE_ALL
    printfT( "mtInitSignals()\n" );
#endif
#if defined(__MT__)
    signal(SIGHUP,   signalHandlerThread );     // hang-up
    signal(SIGPIPE,  signalHandlerThread );     // broken pipe
    signal(SIGSEGV,  signalHandlerThread );
    signal(SIGTERM,  signalHandlerThread );     // kill (lÑ·t der sich doch catchen?)
    signal(SIGUSR1,  signalHandlerThread );
#endif
}   // mtInitSignals



static void mtGetArticle( void *threadNo )
//
//  states:  starting -> running -> waiting|failed            ( !nntpSyncCall)
//           starting -> running -> runningspecial|failed     (  nntpSyncCall)
//
{
    int no = (int)threadNo;
    TNntp::Res res;
    int connectionOk = 1;

#ifdef TRACE_ALL
    printfT( "mtGetArticle(%d): running\n",no );
#endif
    assert( nntpS[no] == starting );
    if ( !nntpSyncCall[no])
	mtInitSignals();

    //
    //  get article
    //
    nntpS[no] = running;
    res = nntp[no].getArticle();
    ++artsGotten;

    //
    //  if successfully retrieved, write article
    //
    switch (res) {
    case TNntp::ok:
	//
	//  article successfully received
	//
	writeArticle( areas,nntp[no].getTmpF(),nntp[no].groupName() );
	newsrc.artMarkRead( nntp[no].groupName(), nntp[no].article() );
	++artsRcvd;
	nntp[no].artNotAvail = 0;
	break;
    case TNntp::killed: {
	newsrc.artMarkRead( nntp[no].groupName(), nntp[no].article() );
	areas.mailPrintf1( OW_NEWS,"article killed in %s:\n%s\n",
			   nntp[no].groupName(), nntp[no].getLastErrMsg() );
	++artsKilled;
	nntp[no].artNotAvail = 0;
	break;
    }
    case TNntp::notavail:
	//
	//  Article not available.  Look for next available article.
	//
	//  Should be required for syncCall's to mtGetGroup only.  Otherwise
	//  not available articles should be detected by getXHdr.
	//  If more than one thread is handling a single group, this next
	//  handling does not (cannot) work correctly (too complicated to implement
	//  correctly, expected gain too small)
	//
	if (doingProcessSendme) {
	    newsrc.artMarkRead( nntp[no].groupName(), nntp[no].article() );
	    areas.mailPrintf1( OW_NEWS,"%s: %s\n",
			       nntp[no].groupName(),nntp[no].getLastErrMsg() );
	}
	else {
	    //
	    //  hier kann was sehr hÑ·liches passieren:
	    //  es wurde noch kein Article gelesen und es wird notavail
	    //  zurÅckgegeben.  Dann gibt NEXT nÑmlich grpLo+irgendwas
	    //  (den zweiten verfÅgbaren Artikel zurÅck).  Was bedeutet,
	    //  da· man sich mit NEXT langsam durch alle gelesenen
	    //  Artikel durchquÑlt, bis man den jetzigen erreicht hat...
	    //  Abhilfe:  nntpArticle<0  ->  noch keinen gelesen...
	    //  Damit die Sache nicht bei 'kleinen' Lîchern in der Newsgroup die
	    //  ganze Zeit in NEXTs verfÑllt, wird mitgezÑhlt, wieviele Artikel
	    //  hintereinander nicht da waren.  Wird ein bestimmter Wert
	    //  Åberschritten, wird mit NEXT weitergearbeitet (vorher nicht!)
	    //
#ifdef DEBUG_ALL
	    printfT( "mtGetArticle(%d): not avail: %ld,%d\n", no,nntp[no].article(), nntp[no].artNotAvail+1 );
#endif
	    if (++nntp[no].artNotAvail < 10)
		newsrc.artMarkRead( nntp[no].groupName(), nntp[no].article() );
	    else if ( !newsrc.artIsRead(nntp[no].groupName(),nntp[no].article())) {
		nntp[no].artNotAvail = 0;
		newsrc.artMarkRead( nntp[no].groupName(), nntp[no].article() );
		if (nntp[no].nntpArticle() >= 0) {
		    long artLo, artHi;
		    long nextArt;

		    artLo = nntp[no].article()+1;
		    artHi = nntp[no].artHi();
		    while (nntp[no].nextArticle(&nextArt) == TNntp::ok) {
			artHi = nextArt-1;
#ifdef DEBUG_ALL
			printfT("mtGetArticle(%d): next! %ld-%ld  \n",no,artLo,artHi);
#endif
			if (nextArt > nntp[no].article())
			    break;
			while (artLo <= artHi)
			    newsrc.artMarkRead( nntp[no].groupName(), artLo++ );
			artLo = artHi+2;
		    }

		    //
		    //  mark articles from artLo..artHi as read
		    //  at least required, if there is no next article
		    //
#ifdef DEBUG_ALL
		    printfT("mtGetArticle(%d): NEXT! %ld-%ld  \n",no,artLo,artHi);
#endif
		    while (artLo <= artHi)
			newsrc.artMarkRead( nntp[no].groupName(), artLo++ );
		}
	    }
	}
	break;
    default:
	areas.mailPrintf1( OW_NEWS,"%s: %s\n",
			   nntp[no].groupName(),nntp[no].getLastErrMsg() );
	connectionOk = 0;
	break;
    }

#ifdef TRACE_ALL
    printfT( "mtGetArticle(%d): finished, %ld\n",no,nntp[no].article() );
#endif
    if (connectionOk) {         // article handling finished
	if (nntpSyncCall[no])
	    nntpS[no] = runningspecial;
	else
	    nntpS[no] = waiting;
    }
    else
	nntpS[no] = failed;     // let connection die
    threadFinito.Post();
}   // mtGetArticle



static void mtGetNewGroups( void *threadNo )
//
//  states:
//
{
    int no = (int)threadNo;
    char nntpTimePath[FILENAME_MAX];

#ifdef TRACE_ALL
    printfT( "mtGetNewGroups(%d)\n",no );
#endif
    assert( nntpS[no] == starting );
    nntpS[no] = runningspecial;

    mtInitSignals();

    sprintfT( nntpTimePath, "%s/%s", homeDir,FN_NEWSTIME );
    if (nntp[no].getNewGroups(nntpTimePath,!readOnly) != TNntp::ok) {
	areas.mailPrintf1( 0,"cannot get new groups:\n        %s\n",
			   nntp[no].getLastErrMsg() );
	areas.mailPrintf1( 0,"        perhaps you should check %s\n",nntpTimePath );
	areas.forceMail();
	nntpS[no] = waiting;
	threadFinito.Post();
	return;
    }

    {
	TFileTmp &in = nntp[no].getTmpF();
	int mailOpened = 0;
	char buf[BUFSIZ];

#ifdef DEBUG
	printfT( "mtGetNewGroups: in Schleife\n" );
#endif
	in.seek(0L, SEEK_SET);
    
	mailOpened = 0;
	while (in.fgets(buf,sizeof(buf),1) != NULL) {
#ifdef DEBUG
	    printfT( "mtGetNewGroups: %s\n",buf );
#endif

	    //
	    //  scan to see if we know about this one
	    //
	    if (newsrc.grpExists(buf))
		continue;
	
	    newsrc.grpAdd( buf );

	    //
	    //  beim ersten neuen Namen eine Mail îffnen
	    //
	    if ( !mailOpened) {
		//
		//  Open message file.
		//
		mailOpened = 1;
		areas.mailStart();
		areas.mailPrintf1( 0,"new newsgroups:\n\n", buf );
		areas.forceMail();                                 // force generation of status mail
	    }

	    //
	    //  neuen Namen in die Mail schreiben
	    //
	    areas.mailPrintf1( 0,"%s\n",buf );
	}
	if (mailOpened)
	    areas.mailStop();
    }

#ifdef DEBUG
    printfT( "mtGetNewGroups(): finished\n" );
#endif
    nntpS[no] = waiting;
    threadFinito.Post();
}   // mtGetNewGroups



//--------------------------------------------------------------------------------



static int mtGetXhdrCallback( int operation, const char *line )
//
//  call back for each line of the XHDR command
//  !! not MT safe !!
//  operation:
//  0   standard operation
//  1   init expNum
//  2   init groupName
//  returns '0', if operation should be aborted (emergency exit only!)
//
{
    long curNum;
    long num;
    static long expNum = 0;
    static const char *groupName;

#ifdef TRACE_ALL
    printfT( "mtGetXhdrCallback(%d,%s)\n",operation,line );
#endif

    switch( operation ) {
    case 1:
	expNum = atol( line );
	break;
    case 2:
	groupName = line;
	break;
    default:
	curNum = atol( line );
	
	for (num = expNum;  num < curNum;  num++) {
	    newsrc.artMarkRead( groupName,num );
#ifdef DEBUG_ALL
	    printfT( "xhdr %ld in %s not available\n",num,groupName );
#endif
	}
	expNum = curNum + 1;
    }
    return !stopOperation;
}   // mtGetXhdrCallback



static void mtGetXhdr( void *threadNo )
//
//  Get XHDRs if supported
//  If successful, the obtained information is used to remove not available
//  articles from newsrc.  Assumption is, that newsserver returns this info
//  in rising order
//
//  *** I am not sure, if this option is really useful ***
//
{
    int no = (int)threadNo;
    TNntp::Res res;

#ifdef TRACE_ALL
    printfT("mtGetXhdr(%d)\n",no );
    printThreadState("mtGetXhdr");
#endif

    xhdrSema.Request();                // weil es die Vars des Callback nur einmal gibt

    assert( nntpS[no] == starting );
    nntpS[no] = runningspecial;

    mtInitSignals();

    res = TNntp::ok;
    if (nntp[no].artHi() - nntp[no].artFirst() > 20)
	res = nntp[no].getXhdr( "LINES",nntp[no].artFirst(),nntp[no].artHi(),
				mtGetXhdrCallback );

    if (res == TNntp::ok)
	nntpS[no] = waiting;
    else
	nntpS[no] = aborted;

    threadFinito.Post();

    xhdrSema.Release();
}   // mtGetXhdr



//--------------------------------------------------------------------------------



static void _nntpMtConnect( void *threadNo )
//
//  set up single connection to news server (could be started as a thread)
//  states:  init -> connecting -> waiting  ||
//           init -> connecting -> failed
//  give it three tries on problem...
//
{
    int i;
    int no = (int)threadNo;
    static int readOnlyDisplayed = 0;
    static int nntpMsgDisplayed = 0;

    assert( nntpS[no] == init );

    mtInitSignals();
    nntp[no].setHelper( doXref ? processXref : NULL, killArticleQ );

    for (i = 0;  i < 3;  i++) {
	nntpS[no] = connecting;

	if (doAbortProgram)
	    break;
	
	if (nntp[no].open(nntpInfo[0].host,nntpInfo[0].user,nntpInfo[0].passwd,nntpInfo[0].port,0) == TNntp::ok) {

	    //
	    //  display, if posting possible or not
	    //
	    if ( !readOnlyDisplayed  &&  nntp[no].isReadOnly()) {
		readOnlyDisplayed = 1;
		areas.mailPrintf1( OW_NEWS,"you cannot post to news server %s\n",nntpInfo[0].host );
	    }

	    //
	    //  display the type of the NNTP server (sometimes useful, esp. for debugging)
	    //  little bit complicated, but a like nice formatting
	    //
	    if ( !nntpMsgDisplayed) {
		char buf[500];
		const char *p;
		size_t sndx;
		
		nntpMsgDisplayed = 1;
		sprintfT( buf,"%.450s",nntp[no].getLastErrMsg() );
		strlwr( buf );    // egal, ob nls berÅcksichtigt wird
		p = strstr(buf,"server");
		sndx = 0;
		sprintfT( buf,"%.450s",nntp[no].getLastErrMsg() );

		if (p != NULL) {
		    size_t ndx;
		    
		    ndx = p-buf;
		    if (ndx > 25) {
			strncpy( buf+ndx-25,"...",3 );
			sndx = ndx-25;
		    }
		    if (strlen(buf) > sndx+60)
			strcpy( buf+sndx+57,"..." );
		}
		areas.mailPrintf1( 0,"server: %s\n", buf+sndx );
	    }
	    nntpS[no] = waiting;
	    break;
	}
	nntpS[no] = failed;
    }
}   // _nntpMtConnect



static void nntpConnect( int maxThreads )
//
//  set up connection to news server
//
{
    int i;

#ifdef TRACE_ALL
    printfT( "nntpConnect(%d)\n",maxThreads );
#endif

    assert( maxThreads <= maxNntpThreads );

    for (i = 0;  i < maxThreads;  i++)
	BEGINTHREAD( _nntpMtConnect, (void *)i );
}   // nntpConnect



static void nntpMtDisconnect( void *maxThreads )
//
//  (explicit) disconnect from news server
//
{
    int maxNo = (int)maxThreads;
    int i;
    
#ifdef TRACE_ALL
    printfT( "nntpDisconnect(%d)\n",maxNo );
#endif

    assert( maxNo <= maxNntpThreads );

    mtInitSignals();
    for (i = 0;  i < maxNo;  i++) {
	if (doAbortProgram)
	    break;
	if (nntpS[i] != failed  &&  nntpS[i] != aborted)
	    nntp[i].close();
	nntpS[i] = init;
    }
    disconnectDone.Post();
}   // nntpDisconnect



static int nntpMtWaitConnect( int maxThreads=maxNntpThreads )
//
//  wait until one of the threads has successfully connected, or all of them
//  have failed.  On failure return 0 (timeout after 60s)
//
{
    int i;
    int conFailed = 0;
    long time = 0;

    assert( maxThreads <= maxNntpThreads );

    while ( !conFailed) {
#ifdef TRACE_ALL
	printThreadState( "nntpMtWaitConnect()",maxThreads );
#endif
	conFailed = 1;
	for (i = 0;  i < maxThreads;  i++) {
	    switch (nntpS[i]) {
	    case running:
	    case runningspecial:
	    case starting:
	    case waiting:
		return 1;                   // -> connected !
		break;
	    case aborted:
	    case failed:
		break;                      // -> do nothing
	    case init:
	    case connecting:
		conFailed = 0;              // -> not failed
		break;
	    }
	}
	//
	//  wait ~100ms
	//
	if ( !conFailed) {
	    _sleep2( 100 );
	    time += 100;
	    conFailed = (time > TIMEOUT*1000);     // timeout after TIMEOUT s
	}
    }
#ifdef TRACE_ALL
    printfT( "nntpMtWaitConnect():  TIMEOUT!\n" );
#endif
    return 0;
}   // nntpMtWaitConnect



static int nntpMtGetWaiting( int wait, NntpStates setState = init )
//
//  look for waiting thread & return ndx
//  return -1, if none is waiting
//  aborted/failed/runningspecial threads are skipped
//  if wait requested, nntpMtGetWaiting loops til it finds a waiting thread
//  (i.e. there is also no progress display)
//
{
    int i;

#ifdef TRACE_ALL
    printThreadState( "nntpMtGetWaiting()",maxNntpThreads );
#endif

    for (;;) {
	blockThread();
	for (i = 0;  i < maxNntpThreads;  i++) {
	    switch (nntpS[i]) {
	    case waiting:
		if (setState != init)
		    nntpS[i] = setState;
		unblockThread();
		return i;
	    default:
		break;
	    }
	}
	unblockThread();
	if ( !wait)
	    return -1;
	threadFinito.Wait( 100 );
    }
}   // nntpMtGetWaiting



static int nntpMtAnyRunning( int checkSpecial, int maxThreads=maxNntpThreads )
//
//  return 1, if one thread is 'running', otherwise 0
//  - aborted/failed threads are skipped
//  - if checkSpecial is activated, the nntpMtAnyRunning return true also, if
//    there is a 'runningspecial' thread.  Otherwise these threads are skipped
//
{
    int i;

#ifdef TRACE_ALL
    printThreadState( "nntpMtAnyRunning()", maxThreads );
#endif

    assert( maxThreads <= maxNntpThreads );

    for (i = 0;  i < maxThreads;  i++) {
	switch (nntpS[i]) {
	case starting:
	case running:
	    return 1;
	case runningspecial:
	    if (checkSpecial)
		return 1;
	    else if (maxThreads <= maxNntpThreads)
		++maxThreads;
	    break;
	case aborted:
	case failed:
	    if (maxThreads <= maxNntpThreads)
		++maxThreads;
	    break;
	default:
	    break;
	}
    }
    return 0;
}   // nntpMtAnyRunning



static void nntpWaitFinished( int maxThreads=maxNntpThreads )
//
//  wait until every operation has stopped
//    
{
    threadFinito.Wait( 500 );     // wait til mtGetGroup() has been started (in any case) - not clean
    stopOperation = 1;

    for (;;) {
	if ( !nntpMtAnyRunning(1,maxThreads)  &&  activeRoutines == 0)
	    break;
	threadFinito.Wait( 500 );
    }
    abortOperation = 1;
}   // nntpWaitFinished



//--------------------------------------------------------------------------------



static void readNewsrc( const char *name )
{
    if ( !newsrc.readFile(name))
	areas.mailPrintf1( OW_NEWS,"there is no %s file\n",name );
}   // readNewsrc



static int nntpConnected( void )
{
    int ok,i;

#ifdef TRACE_ALL
    printThreadState("nntpConnected()");
#endif

    ok = 0;
    for (i = 0;  i < maxNntpThreads;  ++i) {
	if (nntpS[i] == waiting  ||  nntpS[i] == aborted)   // the aborted threads were also connected successfully...
	    ++ok;
    }
    return ok;
}   // nntpConnected



static void statusInfo( int artRead=0 )
//
//  write status info to mail file (and also to console).
//  status info contains articles read/killed etc.
//
{
    int ok;

    OutputCR( OW_NEWS );
    OutputClrEol( OW_NEWS );
    
    if (artRead) {
	char msg1[80];
	char msg2[80];

	sprintfT( msg1,"%ld article%s read",
		  (long)artsRcvd, (artsRcvd != 1) ? "s" : "" );
	msg2[0] = '\0';
	if (artsKilled != 0)
	    sprintfT( msg2, ", %ld article%s killed",
		      (long)artsKilled,(artsKilled != 1) ? "s" : "" );
	areas.mailPrintf1( OW_NEWS,"\n" );
	areas.mailPrintf1( OW_NEWS,"%s%s\n", msg1,msg2 );
    }

    ok = nntpConnected();
    areas.mailPrintf1( OW_NEWS,"%d thread%s %s connected successfully\n",
		       ok,(ok != 1) ? "s" : "",(ok != 1) ? "were" : "was" );
}   // statusInfo



static int checkNntpConnection( int maxThreads, const char *msg )
//
//  check connection to NNTP server
//  if failed return 0, on success return 1
//
{
#ifdef TRACE_ALL
    printfT( "checkNntpConnection(%d)\n",maxThreads );
#endif
    if ( !nntpMtWaitConnect(maxThreads)) {
	areas.mailPrintf1( OW_NEWS,"cannot connect to news server %s (%s):\n        %s\n",
			   (nntpInfo[0].host != NULL) ? nntpInfo[0].host : "\b", msg,
			   nntp[0].getLastErrMsg() );
	return 0;
    }
    areas.mailPrintf1( OW_NEWS,"connected to news server %s (%s)\n",
		       nntpInfo[0].host,msg );
    return 1;
}   // checkNntpConnection



//--------------------------------------------------------------------------------



static void mtGetGroup( void *threadNo )
//
//  Get articles from the newsgroup.
//  Return TRUE if successful.
//  threadNo must be an available thread, the groupName must have been
//  entered in nntp[thread]
//
{
    int thread = (int)threadNo;
    long grpCnt, grpLo, grpHi, grpFirst, artNum;
    int killEnabled;
    int artRequested;
    int somethingDone;
    const char *groupName = NULL;
    int syncCall = nntpSyncCall[thread];   // indicates synccalling of getArticle()
    int gotSyncTransSema = 0;

#ifdef TRACE_ALL
    printfT( "mtGetGroup(%s), thread %d\n",nntp[thread].groupName(),thread );
#endif

    if (syncCall)
	mtInitSignals();

    ++activeRoutines;

    assert( thread >= 0 );
    xstrdup( &groupName, nntp[thread].groupName() );
    
    //
    //  Select group name from news server.
    //
    if (nntp[thread].setActGroup(groupName, grpCnt,grpLo,grpHi) != TNntp::ok) {
	areas.mailPrintf1( OW_NEWS,"cannot select %s:\n        %s\n        Unsubscribe group manually\n",
			   groupName,nntp[thread].getLastErrMsg() );
	areas.forceMail();
	delete groupName;
	nntpS[thread] = waiting;
	--activeRoutines;
	threadFinito.Post();
	return;
    }

    killEnabled = killF.doKillQ( groupName );
#ifdef DEBUG_ALL
    printfT( "mtGetGroup: killEnabled=%d\n",killEnabled );
#endif
    
    //
    //  Fix the read article number list (with lo/hi received thru group selection)
    //
    newsrc.grpFixReadList( groupName,grpLo,grpHi,initialCatchupCount );

#ifdef DEBUG_ALL
    printfT( "group selected: %s %ld-%ld\n",groupName,grpLo,grpHi );
#endif

    grpFirst = newsrc.grpFirstUnread( groupName,grpLo );
#ifdef DEBUG_ALL
    printfT( "first unread: %ld\n",grpFirst );
#endif
    {
	//
	//  calculate number of articles to fetch (pessimistic version)
	//  and display it.
	//
	long artCnt = grpHi-grpFirst+1;

	if (artCnt < 0)
	    artCnt = 0;

	if (syncCall)                 // for display only
	    artsTot += artCnt;
	else {
	    blockThread();
	    artsTot = (artsTot-artsGotten) + artCnt;
	    artsGotten = 0;
	    unblockThread();
	}
#ifdef DEBUG
	areas.mailPrintf1( 0,"%s: %ld %ld %ld %ld\n", groupName,grpFirst,grpHi,artCnt,grpCnt );
#endif

	if (grpHi-grpLo+1 != grpCnt  &&  artCnt > grpCnt)
	    artCnt = grpCnt;

	areas.mailPrintf1( OW_NEWS,"%4ld unread article%c in %s\n", artCnt,
			   (artCnt == 1) ? ' ' : 's', groupName);
#ifdef DEBUG_ALL
	areas.mailPrintf1( 0,"1: %ld\n", artCnt );
#endif
    }

    //
    //  get the XHDRs (performace hit, if many holes in the article sequence)
    //
    if ( !syncCall) {
	nntp[thread].selectArticle( groupName,grpFirst,killEnabled,grpFirst,grpHi );
	BEGINTHREAD( mtGetXhdr, (void *)thread );   // after mtGetXHdr() thread state is waiting
    }
    
    //
    //  Look through unread articles
    //  (just a service to wait for a 'waiting' thread)
    //
    artNum = grpFirst;
    artRequested = 1;
    while (artNum <= grpHi  ||  !artRequested  ||
	   (nntpMtGetWaiting(0) < 0  &&  !syncCall)) {

	//
	//  should we make transition from syncCall ?
	//  Note:  this should be done only by one mtGetGroup()-thread.  Otherwise
	//         it is possible that the several connected threads are changing GROUP
	//         assignment on each article (speed loss)
	//
	if (stopOperation  &&  syncCall) {
#ifdef DEBUG_ALL
	    printfT( "mtGetGroup(): transition to stopOperation\n" );
#endif
	    if (syncTransSema.Request(0)) {
		gotSyncTransSema = 1;
		syncCall = 0;
		nntpS[thread] = waiting;
	    }
	}

	//
	//  find next unread article number
	//
	while (artRequested  &&  artNum <= grpHi) {
	    if (newsrc.artIsRead(groupName,artNum)) {
#ifdef DEBUG_ALL
		printfT( "skip! %ld  \n",artNum );  ////
#endif
		++artNum;
		++artsGotten;
	    }
	    else
		artRequested = 0;
	}

	somethingDone = 0;

	//
	//  if there is a waiting thread, then receive the next article with that one
	//
	if ( !artRequested) {
	    if ( !syncCall) {
		thread = nntpMtGetWaiting(0,starting);
		if (thread >= 0) {
		    nntpSyncCall[thread] = syncCall;
		    nntp[thread].selectArticle( groupName,artNum,killEnabled );
		    BEGINTHREAD( mtGetArticle,(void *)thread );
		    somethingDone = 1;
		}
	    }
	    else {
		nntpS[thread] = starting;
		nntpSyncCall[thread] = syncCall;
		nntp[thread].selectArticle( groupName,artNum,killEnabled );
		if (nntpS[thread] == failed  ||  nntpS[thread] == aborted)
		    artNum = grpHi+1;
		else
		    mtGetArticle( (void *)thread );
		somethingDone = 1;
	    }
	    if (somethingDone) {
		artRequested = 1;
		++artNum;
	    }
	}

	if ( !somethingDone)
	    threadFinito.Wait( 500 );

	//
	//  Check if too many blocks already
	//
	if (enoughRcvdQ())
	    artNum = grpHi+1;    // trick: initiation of article reading disabled
    }

    assert( artNum > grpHi );
    assert( artRequested );
    
    if (syncCall)
	nntpS[thread] = waiting;
    if (gotSyncTransSema)
	syncTransSema.Release();

#ifdef TRACE_ALL
    printfT( "mtGetGroup3(%s) finished\n",groupName );
#endif

    delete groupName;
    --activeRoutines;
    threadFinito.Post();
    return;
}   // mtGetGroup



//--------------------------------------------------------------------------------
//
//  handle COMMANDS file
//



static int processSendme( TFile &cmdF )
{
    long grpCnt, grpLo, grpHi;
    long artLo, artHi;
    int thread;
    int finished;
    int artRequested;
    int somethingDone;
    char buf[BUFSIZ];
    const char *groupName;
    int killEnabled;

#ifdef TRACE_ALL
    printfT( "processSendme()\n" );
#endif

    //
    //  Read newsgroup name.
    //
    if (cmdF.scanf("%s", buf) != 1) {
	cmdF.fgets(buf, sizeof(buf), 1);
	return 0;
    }
    groupName = xstrdup( buf );

    thread = nntpMtGetWaiting( 1,starting );
#ifdef TRACE_ALL
    printfT( "thread: %d\n",thread );
#endif

    //
    //  Select group name from news server.
    //
    if (nntp[thread].setActGroup(groupName, grpCnt,grpLo,grpHi) != TNntp::ok) {
	areas.mailPrintf1( OW_NEWS,"cannot select %s:\n        %s\n        Unsubscribe group manually\n",
			   groupName,nntp[thread].getLastErrMsg() );
	areas.forceMail();
	cmdF.fgets(buf, sizeof(buf), 1);
////    delete groupName;
	nntpS[thread] = waiting;
	return 0;
    }
    nntpS[thread] = waiting;

    //
    //  if group does not exist in newsrc, add it
    //
    if ( !newsrc.grpExists(groupName)) {
	newsrc.grpAdd( groupName,1 );
	areas.mailPrintf1( OW_NEWS,"%s added to %s\n",groupName,newsrcFile );
	areas.forceMail();
    }

    //
    //  rem:  if articles are selected manually, we assume, that
    //        the user knows which article he/she selects...
    //
//    killEnabled = killF.doKillQ( groupName );
    killEnabled = 0;

    //
    //  Fix the read article number list
    //
    newsrc.grpFixReadList( groupName, grpLo, grpHi);

#ifdef DEBUG_ALL
    printfT( "group selected: %s %ld-%ld\n",groupName,grpLo,grpHi );
#endif

    areas.mailPrintf1( OW_NEWS,"%s selected\n", groupName );
#ifdef DEBUG_ALL
    areas.mailPrintf1( OW_NEWS,"     %ld-%ld\n",grpLo,grpHi );
#endif

    //
    //  get the articles
    //  (just a service to wait for a 'waiting' thread)
    //
    finished = 0;
    artRequested = 1;
    artLo = artHi = 0;
    while ( !finished  ||  !artRequested  ||  nntpMtGetWaiting(0) < 0) {
#ifdef DEBUG_ALL
	printThreadState( "processSendme()" );
#endif

	//
	//  get next article number (if any exists)
	//
	while (artRequested  &&  !finished) {
	    if (artLo < artHi) {
		++artLo;
		if ( !newsrc.artIsRead(groupName,artLo))
		    artRequested = 0;
	    }
	    else {
		if (cmdF.scanf("%*[ \t]%[0-9]", buf) == 0) {
		    cmdF.fgets(buf, sizeof(buf), 1);
		    finished = 1;
		}
		else {
		    artLo = artHi = atol(buf);
		    if (cmdF.scanf("-%[0-9]", buf) == 1)
			artHi = atol(buf);
		    if (artLo >= 0) {
			if ( !newsrc.artIsRead(groupName,artLo))
			    artRequested = 0;
		    }
		}
	    }
	}

	somethingDone = 0;

	//
	//  if there is a waiting thread, then receive the article with that one
	//
	if ( !artRequested) {
	    thread = nntpMtGetWaiting( 0,starting );
	    if (thread >= 0) {
#ifdef DEBUG_ALL
	    printfT( "sendme: %ld, fini='%d'\n",artLo,finished );
#endif
		nntp[thread].selectArticle( groupName,artLo,killEnabled );
		nntpSyncCall[thread] = 0;
		BEGINTHREAD( mtGetArticle,(void *)thread );
		artRequested = 1;
		somethingDone = 1;
	    }
	}

	if ( !somethingDone) {
	    artsTot = artLo;
	    threadFinito.Wait( 500 );
	}

	//
	//  check if too many block already received
	//
	if (enoughRcvdQ()) {
	    cmdF.fgets(buf, sizeof(buf), 1);
	    finished = 1;     // trick:  stop further reading of file...
	}
    }

#ifdef TRACE_ALL
    printThreadState( "processSendme() finished" );
#endif

    assert( artRequested );
////    delete groupName;
    return 1;
}   // processSendme



//--------------------------------------------------------------------------------



int getNews( int strategy )
//
//  If a COMMANDS file exists in the current directory, fetch the articles
//  specified by the sendme commands in the file, otherwise fetch unread
//  articles from newsgroups listed in the newsrc file.
//
//  strategy (2 is applicable only for normal fetching):
//  0:    fetch one group after the other without intersection
//  1:    already start reading next group, one thread available
//  2:    fetch groups in parallel
//  0,1:  all connected threads are receiving one group with max speed, which
//        could mean, that some threads are waiting til end of group
//  2:    all threads are kept busy
//  speed increases from 0..2 (especially for many small groups)
//  danger of receiving crossposted articles increases from 0..2
//
{
    TFile cmdF;

    //
    //  start connecting to nntpServer
    //
    nntpConnect( maxNntpThreads );

    //
    //  Read .newsrc file (may take a while)
    //
    readNewsrc(newsrcFile);

    //
    //  Read kill file (error msg only, if file was given thru cmdline parameter)
    //
    if (killF.readFile(killFile) == -1  &&  killFileOption) {
	areas.mailPrintf1( OW_NEWS,"kill file %s not found.\n", killFile );
	areas.forceMail();
    }

    //
    //  check connection
    //
    if ( !checkNntpConnection(maxNntpThreads,"getNews"))
	return 0;

#ifdef DEBUG_ALL
    printfT( "waiting: %d\n", nntpMtGetWaiting(1) );
#endif

    //
    //  Check for new newsgroups.
    //
    if (doNewGroups) {
	int thread = nntpMtGetWaiting(1,starting);
	BEGINTHREAD( mtGetNewGroups, (void *)thread );
    }
    nntpMtGetWaiting(1);

#ifdef __MT__
    BEGINTHREAD( mtThroughputInfo, (void *)500 );
#endif

    artsRcvd = 0;
    artsKilled = 0;

    if (cmdF.open(FN_COMMAND,TFile::mread,TFile::otext)) {
	//
	//  Process command file containing sendme commands.
	//
	char buf[BUFSIZ];
	int  aborted = 0;

	doingProcessSendme = 1;
	while (cmdF.scanf("%s", buf) == 1) {
	    if (stricmp(buf, "sendme") == 0) {
		processSendme(cmdF);
		while (strategy == 0  &&  nntpMtAnyRunning(0))
		    threadFinito.Wait( 500 );
	    }
	    else {
		areas.mailPrintf1( OW_NEWS,"ill command in %s file: %s\n",
				   FN_COMMAND,buf );
		areas.forceMail();
		cmdF.fgets(buf, sizeof(buf), 1);
	    }
	    if (enoughRcvdQ()) {
		aborted = 1;
		break;
	    }
	}
	if ( !readOnly  &&  !aborted)
	    cmdF.remove();
	else
	    cmdF.close();
    } else {
	//
	//  For each subscribed newsgroup in .newsrc file
	//
	const char *groupName;

	doingProcessSendme = 0;
	groupName = newsrc.grpFirst();
	while (groupName != NULL) {
	    int thread;

	    assert( newsrc.grpSubscribed(groupName) );
	    thread = nntpMtGetWaiting( 1,starting );
	    if (enoughRcvdQ())
		break;
	    nntp[thread].selectArticle( groupName );
	    if (strategy == 2) {
		nntpSyncCall[thread] = 1;
		BEGINTHREAD( mtGetGroup, (void *)thread );
	    }
	    else {
		nntpSyncCall[thread] = 0;
		mtGetGroup( (void *)thread );
		while (strategy == 0  &&  nntpMtAnyRunning(0))
		    threadFinito.Wait( 500 );
	    }
	    if (enoughRcvdQ())
		break;
	    groupName = newsrc.grpNext( groupName );
	}
    }

    nntpWaitFinished();

    BEGINTHREAD( nntpMtDisconnect,(void *)maxNntpThreads );

    statusInfo(1);

    if ( !readOnly)
	newsrc.writeFile();

    disconnectDone.Wait( 5000 );    // wait for disconnect (maximum of 5s)
    return 1;
}   // getNews



//--------------------------------------------------------------------------------



static char *nextField(char **ppCur)
//
//  Return next field in record.  Returns NULL, if there is no nextField
//
{
    char *pEnd;
    char *pStart = *ppCur;

    if (pStart == NULL)
        return NULL;
    
    if ((pEnd = strchr(pStart, '\t')) != NULL) {
	*pEnd++ = '\0';
	*ppCur = pEnd;
    }
    else
        *ppCur = NULL;
    
    return pStart;
}   // nextField



static void mtSumGroup( void *threadNo )
{
    int no = (int)threadNo;
    long grpCnt,grpLo,grpHi,grpFirst;
    int sumAborted = 0;

#ifdef TRACE
    printfT( "mtSumGroup(%d)\n",no );
#endif
    assert( nntpS[no] == starting );

    mtInitSignals();

    if (nntp[no].setActGroup( nntp[no].groupName(), grpCnt,grpLo,grpHi ) != TNntp::ok) {
	areas.mailPrintf1( OW_NEWS,"cannot select %s (sumnews):\n        %s\n",
			   nntp[no].groupName(), nntp[no].getLastErrMsg() );
	areas.forceMail();
	goto THREAD_FINISHED;
    }

    //
    //  Fix up the read article number list
    //
    newsrc.grpFixReadList( nntp[no].groupName(),grpLo,grpHi,initialCatchupCount );
    grpFirst = newsrc.grpFirstUnread( nntp[no].groupName(),grpLo );
    {
	//
	//  calculate number of articles to fetch (pessimistic version)
	//
	long artCnt = grpHi-grpFirst+1;
	
	if (grpHi-grpLo+1 != grpCnt) {
	    if (artCnt > grpCnt)
		artCnt = grpCnt;
	}
	grpCnt = artCnt;
    }
    areas.mailPrintf1( OW_NEWS,"%4ld unread article%c in %s (sumnews)\n", grpCnt,
		       (grpCnt == 1) ? ' ' : 's', nntp[no].groupName());

    if (grpFirst > grpHi)
	goto THREAD_FINISHED;

    nntpS[no] = running;
    if (nntp[no].getOverview(grpFirst,grpHi) != TNntp::ok) {
	areas.mailPrintf1( OW_NEWS,"sumnews of %s aborted:\n        %s\n",
			   nntp[no].groupName(), nntp[no].getLastErrMsg() );
        areas.forceMail();
        sumAborted = 1;
    }

    //
    //  write the collected data to index file
    //
    {
	TFileTmp &inF = nntp[no].getTmpF();
        char buf[BUFSIZ];
        long artNumOk = -1;     // indicates empty result

#ifdef TRACE_ALL
	printfT( "writing idx of %s\n",nntp[no].groupName() );
#endif
        inF.seek( 0L, SEEK_END );
        if (inF.tell() > 0) {
            areas.msgStart( nntp[no].groupName(), "ic" );
            
            inF.seek( 0L, SEEK_SET );
            while (inF.fgets(buf,sizeof(buf),1) != NULL) {
                char *cur = buf;
                long artNum;
                
                artNum = atol(nextField(&cur));          	// article number
                if ( !newsrc.artIsRead(nntp[no].groupName(),artNum)) {
                    char *subject, *from, *date, *mid, *ref, *bytes, *lines;
                    subject = nextField(&cur);
                    from    = nextField(&cur);
                    date    = nextField(&cur);
                    mid     = nextField(&cur);
                    ref     = nextField(&cur);
                    bytes   = nextField(&cur);
                    lines   = nextField(&cur);
                    areas.msgPrintf( "\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%ld\n",
                                     subject, from, date, mid, ref,
                                     (*bytes != '\0'  &&  *bytes != ' ') ? bytes : "0",
                                     lines, artNum);
                    if (lines != NULL)
                        artNumOk = artNum;
                }
            }
            
            if ( !sumAborted)
                newsrc.grpCatchup( nntp[no].groupName(),grpLo,grpHi,0 );
            else if (artNumOk > 0)   // aborted with result
                newsrc.grpCatchup( nntp[no].groupName(),grpLo,artNumOk,0 );
            
            areas.msgStop();
        }
#ifdef TRACE_ALL
	printfT( "writing done of %s\n",nntp[no].groupName() );
#endif
    }

THREAD_FINISHED:
    nntpS[no] = waiting;
    threadFinito.Post();
}   // mtSumGroup



static void sumGroup( const char *groupName )
//
//
{
#ifdef TRACE
    printfT( "sumGroup(%s)\n",groupName );
#endif
    if (groupName != NULL) {
	int thread = nntpMtGetWaiting( 1,starting );
	nntp[thread].selectArticle( groupName );
	BEGINTHREAD( mtSumGroup,(void *)thread );
    }

    while ((groupName != NULL  &&  nntpMtGetWaiting(0) < 0)  ||
	   (groupName == NULL  &&  nntpMtAnyRunning(0))) {
	threadFinito.Wait( 500 );
    }
}   // sumGroup



int sumNews( void )
//
//  Create news summary.
//
{
    const char *groupName;

    //
    //  start connecting to nntpServer
    //
    nntpConnect( maxNntpThreads );

    //
    //  Read .newsrc file (may take a while)
    //
    readNewsrc(newsrcFile);

    //
    //  check connection
    //
    if ( !checkNntpConnection(maxNntpThreads,"sumNews"))
	return 0;
    nntpMtGetWaiting(1);

#ifdef __MT__
    BEGINTHREAD( mtThroughputInfo, (void *)500 );
#endif

    //
    //  For each subscribed newsgroup in the .newsrc file
    //
    groupName = newsrc.grpFirst();
    while (groupName != NULL) {
	assert( newsrc.grpSubscribed(groupName) );
	sumGroup( groupName );
	groupName = newsrc.grpNext( groupName );
    }
    sumGroup( NULL );

    BEGINTHREAD( nntpMtDisconnect, (void *)maxNntpThreads );
    statusInfo(0);
    if ( !readOnly)
	newsrc.writeFile();
    disconnectDone.Wait( 5000 );
    return 1;
}   // sumNews



//--------------------------------------------------------------------------------



static void mtCatchup( void *threadNo )
{
    int no = (int)threadNo;
    long grpCnt,grpLo,grpHi;

#ifdef TRACE
    printfT( "mtCatchup(%d)\n",no );
#endif
    assert( nntpS[no] == starting );

    mtInitSignals();

    nntpS[no] = running;

    if (nntp[no].setActGroup( nntp[no].groupName(), grpCnt,grpLo,grpHi ) != TNntp::ok) {
	areas.mailPrintf1( OW_NEWS,"cannot select %s (catchup):\n        %s\n",
			   nntp[no].groupName(), nntp[no].getLastErrMsg() );
	areas.forceMail();
    }
    else {
	//
	//
	//  catch up the read article number list
	//
	newsrc.grpCatchup( nntp[no].groupName(), 1,grpHi,catchupNumKeep );
	areas.mailPrintf1( OW_NEWS,"catch up %s:  %ld-%ld\n",
			   nntp[no].groupName(),grpLo,grpHi );
    }

    nntpS[no] = waiting;
    threadFinito.Post();
}   // mtCatchup



int catchupNews( long numKeep )
//
//  Catch up in subscribed newsgroups.
//
{
    const char *groupName;

    catchupNumKeep = numKeep;             // nicht besonders fein...

    //
    //  start connecting to nntpServer
    //
    nntpConnect( maxNntpThreads );

    //
    //  read .newsrc file (may take a while)
    //
    readNewsrc(newsrcFile);

    //
    //  check connection
    //
    if ( !checkNntpConnection(maxNntpThreads,"catchupNews"))
	return 0;
    nntpMtGetWaiting(1);

    //
    //  For each subscribed newsgroup in the .newsrc file
    //
    groupName = newsrc.grpFirst();
    while (groupName != NULL) {
	int thread;

	assert( newsrc.grpSubscribed(groupName) );

	thread = nntpMtGetWaiting( 1,starting );
	nntp[thread].selectArticle( groupName );
	BEGINTHREAD( mtCatchup,(void *)thread );

	groupName = newsrc.grpNext( groupName );
    }

    nntpWaitFinished();

    BEGINTHREAD( nntpMtDisconnect, (void *)maxNntpThreads );
    statusInfo(0);

    if ( !readOnly)
	newsrc.writeFile();

    disconnectDone.Wait( 5000 );
    return 1;
}   // catchupNews



//--------------------------------------------------------------------------------



long getNewsStatus( void )
//
//  check, if there are articles waiting for downloading
//
{
    const char *groupName;
    int thread;
    long artsTotal;

    //
    //  start connecting to nntpServer (only one thread!)
    //
    nntpConnect( 1 );

    //
    //  read .newsrc file (may take a while)
    //
    readNewsrc(newsrcFile);

    //
    //  check connection
    //
    if ( !checkNntpConnection(maxNntpThreads,"getNewsStatus"))
	return -1;
    nntpMtGetWaiting(1);

    //
    //  For each subscribed newsgroup in the .newsrc file
    //
    artsTotal = 0;
    thread = nntpMtGetWaiting( 1,starting );
    groupName = newsrc.grpFirst();
    while (groupName != NULL) {
	long grpCnt, grpLo, grpHi;

	assert( newsrc.grpSubscribed(groupName) );

	nntp[thread].selectArticle( groupName );
	if (nntp[thread].setActGroup( groupName, grpCnt,grpLo,grpHi ) != TNntp::ok) {
	    areas.mailPrintf1( OW_NEWS,"cannot select %s (getNewsStatus):\n        %s\n",
			       groupName, nntp[thread].getLastErrMsg() );
	    areas.forceMail();
	}
	else {
	    long grpFirst;
	    long cnt;
	    
	    //
	    //
	    //  display article count
	    //
	    newsrc.grpFixReadList( groupName, grpLo,grpHi );
	    grpFirst = newsrc.grpFirstUnread( groupName, grpLo );
	    cnt = grpHi - grpFirst + 1;
	    if (cnt < 0)
		cnt = 0;
	    artsTotal += cnt;
	    areas.mailPrintf1( OW_NEWS,"%4ld unread article%s in %s\n",
			       cnt,(cnt!=1) ? "s":" ",groupName );
	}

#ifdef DEBUG
	printfT( "%s\n",groupName );
#endif

	groupName = newsrc.grpNext( groupName );
    }

    areas.mailPrintf1( OW_NEWS,"appr. %ld article%s waiting for download\n",
		       artsTotal, (artsTotal!=1) ? "s" : "" );

    BEGINTHREAD( nntpMtDisconnect, (void *)maxNntpThreads );

    disconnectDone.Wait( 5000 );
    return artsTotal;
}   // getNewsStatus
