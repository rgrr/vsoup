//  $Id: main.cc 1.41 1999/08/29 11:08:20 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  Fetch mail and news using POP3 and NNTP into a SOU packet.
//

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

//
//  fÅr TCPOS2.INI-Leserei
//
#ifdef OS2
#define INCL_WINSHELLDATA
#include <os2.h>
#endif

#define DEFGLOBALS
#include <rgmts.hh>
#include <rgsocket.hh>

#include "areas.hh"
#include "global.hh"
#include "news.hh"
#include "pop3.hh"
#include "reply.hh"
#include "util.hh"
#include "output.hh"


static enum { RECEIVE, SEND, CATCHUP, QUERY } mode = RECEIVE;
static char doMail = 1;
static char doNews = 1;
static char doXmt  = 0;
static char doSummary = 0;
static int  catchupCount;
static clock_t startTime;
static long minThroughput = 0;
static int  threadCntGiven = 0;
static int  newsStrategy = 1;
static int  newsXmtStrategy = 1;  // 1=Standard, 0=Push
#ifdef OS2
static char doIni = 1;		  // if TRUE, read TCPOS2.INI file
#endif

static int opt_m = 0;             // new command line interface (indicated with -Z)
static int opt_n = 0;
static int opt_s = 0;
static int opt_Z = 0;


//
//  termination flags & result
//
static int finishedMail = 1;
static int finishedNews = 1;
static int finishedXmt  = 1;
static int retcode = EXIT_SUCCESS;



static void prepareAbort( void ) /*fold00*/
{
    doAbortProgram = 1;
    TSocket::abortAll();
}   // prepareAbort



static void setFiles( void ) /*fold00*/
{
#if defined(OS2)  ||  defined(__WIN32__)
    sprintfT(newsrcFile, "%s/newsrc", homeDir);
    sprintfT(killFile, "%s/score", homeDir);
#else
    sprintfT(newsrcFile, "%s/.newsrc", homeDir);
    sprintfT(killFile, "%s/.score", homeDir);
#endif
}   // setFiles



static void getUrlInfo( const char *url, serverInfo &info ) /*fold00*/
//
//  Get an URL in the form [user[:passwd]@]host[:port]
//  This routine changes only existing fields in 'info'.
//  restrictions:
//  - not optimized for speed
//  - single field length limited to 512 chars (not checked)
//  - portno 0 not allowed
//  - can be fooled easily (with illegal URL specs)
//
{
    char user[512];
    char host[512];
    char username[512];
    char passwd[512];
    char hostname[512];
    char port[512];

    if (sscanfT(url,"%[^@]@%s",user,host) == 2) {
	if (sscanfT(user,"%[^:]:%s",username,passwd) == 2) {
	    xstrdup( &info.user,username );
	    xstrdup( &info.passwd,passwd );
	}
	else
	    xstrdup( &info.user,user );
    }
    else
	strcpy( host,url );

    if (sscanfT(host,"%[^:]:%s",hostname,port) == 2) {
	xstrdup( &info.host,hostname );
	if (atoi(port) != 0)
	    info.port = atoi(port);
    }
    else
	xstrdup( &info.host,host );

#ifdef TRACE
    printfT( "getUrlInfo(%s)(0): %s:%s@%s:%d\n",url,info.user,info.passwd,info.host,info.port );
#endif
    urlDecode( (char *)info.user );
    urlDecode( (char *)info.passwd );
    urlDecode( (char *)info.host );
#ifdef TRACE
    printfT( "getUrlInfo(%s)(1): %s:%s@%s:%d\n",url,info.user,info.passwd,info.host,info.port );
#endif
}   // getUrlInfo



static void checkTransfer( void * ) /*fold00*/
//
//  check transfer...
//
{
    long timeCnt;
    long throughputCnt;
    int  throughputErr;
    long oldXfer, curXfer;

    timeCnt = 0;
    oldXfer  = 0;
    throughputCnt = 0;
    throughputErr = 6;
    for (;;) {
	sleep( 1 );
	if (doAbortProgram)
	    break;
	
	++timeCnt;
	curXfer = TSocket::getTotalBytesRcvd() + TSocket::getTotalBytesXmtd();

	//
	//  check connect timeout
	//
        if (curXfer < 100  &&  timeCnt > 100) {
            areas.mailPrintf1( OW_NEWS,"\n" );
            areas.mailPrintf1( OW_NEWS,"\n" );
	    areas.mailPrintf1( OW_NEWS,"cannot connect -> signal %d generated\n",
			       SIGINT );
#ifdef TRACE_ALL
	    printfT( "checkTransfer():  TIMEOUT\n" );
#endif
#ifdef NDEBUG
	    kill( getpid(),SIGINT );
#else
	    areas.mailPrintf1( OW_NEWS,"NDEBUG not defined -> no signal generated\n" );
#endif
	    break;
	}

	//
	//  check throughput
	//  6 times lower than minThroughput -> generate signal
	//  - minThroughput is mean transferrate over 10s
	//  - minThroughput == -1  ->  disable throughput checker
	//
	++throughputCnt;
	if (curXfer == 0)
	    throughputCnt = 0;
	if (throughputCnt >= 10) {
	    long xfer = curXfer-oldXfer;

	    if (xfer/throughputCnt > labs(minThroughput)  ||  minThroughput == -1)
		throughputErr = 6;
	    else {
		--throughputErr;
		if (throughputErr < 0) {
                    areas.mailPrintf1( OW_NEWS,"\n" );
                    areas.mailPrintf1( OW_NEWS,"\n" );
		    areas.mailPrintf1( OW_NEWS,"throughput lower/equal than %ld bytes/s -> signal %d generated\n",
				       labs(minThroughput),SIGINT );
#ifdef TRACE_ALL
		    printfT( "checkTransfer():  THROUGHPUT\n" );
#endif
#ifdef NDEBUG
		    kill( getpid(),SIGINT );
#else
                    areas.mailPrintf1( OW_NEWS,"NDEBUG not defined -> no signal generated\n" );
		    throughputCnt = 30;
#endif
		    break;
		}
	    }
	    if (minThroughput < -1)
		areas.mailPrintf1( OW_NEWS,"throughput: %lu bytes/s, %d fail sample%s left\n",
				   xfer/throughputCnt,throughputErr,
				   (throughputErr == 1) ? "" : "s");
	    oldXfer = curXfer;
	    throughputCnt = 0;
	}
    }
}   // checkTransfer



static void openAreasAndStartTimer( int argc, char **argv ) /*FOLD00*/
{
    int i;
    char serverNames[BUFSIZ];

    //
    //  put for operation important server names into status mail header
    //
    strcpy( serverNames,"" );
    if (mode == CATCHUP  ||  mode == SEND  ||  (mode == RECEIVE  &&  doNews)  ||  (mode == QUERY  &&  doNews)) {
	for (i = 0;  i < nntpXmtCnt;  ++i) {
	    strcat( serverNames,nntpInfo[i].host );
	    strcat( serverNames," " );
	}
    }
    if (pop3Info.host != NULL  &&  doMail) {
	strcat( serverNames,pop3Info.host );
	strcat( serverNames," " );
    }
    if (smtpInfo.host != NULL  &&  doXmt)
	strcat( serverNames,smtpInfo.host );
    areas.mailOpen( serverNames );

    //
    //  write all given paramters into StsMail (no passwords...)
    //
    areas.mailStart();
    areas.mailPrintf( "%s started\n", VERSION );
    for (i = 0;  i < argc;  ++i) {
	char *s = strstr(argv[i],"://");
	if (s == NULL)
	    areas.mailPrintf( "%s%s", (i == 0) ? "" : " ", argv[i] );
	else
	    areas.mailPrintf( "%s%.*s...", (i == 0) ? "" : " ",s-argv[i]+3,argv[i] );
    }
    areas.mailPrintf( "\n" );
    areas.mailPrintf( "home directory: %s\n", homeDir );
    areas.mailPrintf( "newsrc file:    %s\n", newsrcFile );
    areas.mailPrintf( "kill file:      %s\n", killFile );
    if (mode == CATCHUP  ||  doXmt  ||  doNews) {
        if (nntpXmtCnt == 1) {
            areas.mailPrintf( "nntp server:    nntp://%s%s%s",
                              (nntpInfo[0].user != NULL) ? nntpInfo[0].user : "",
                              (nntpInfo[0].user != NULL) ? ":*@" : "", nntpInfo[0].host );
            if (nntpInfo[0].port != -1)
                areas.mailPrintf( ":%d", nntpInfo[0].port );
            areas.mailPrintf( "\n" );
        }
        else {
            for (i = 0;  i < nntpXmtCnt;  ++i) {
                areas.mailPrintf( "nntp server%d:   nntp://%s%s%s", i,
                                  (nntpInfo[i].user != NULL) ? nntpInfo[i].user : "",
                                  (nntpInfo[i].user != NULL) ? ":*@" : "", nntpInfo[i].host );
                if (nntpInfo[i].port != -1)
                    areas.mailPrintf( ":%d", nntpInfo[i].port );
                areas.mailPrintf( "\n" );
            }
        }
    }
    if (doMail) {
	areas.mailPrintf( "pop3 server:    pop3://%s%s%s",
			  (pop3Info.user != NULL) ? pop3Info.user : "",
			  (pop3Info.user != NULL) ? ":*@" : "", pop3Info.host );
	if (pop3Info.port != -1)
	    areas.mailPrintf( ":%d", pop3Info.port );
	areas.mailPrintf( "\n" );
    }
    if (doXmt) {
	areas.mailPrintf( "smtp gateway:   smtp://%s", smtpInfo.host );
	if (smtpInfo.port != -1)
	    areas.mailPrintf( ":%d", smtpInfo.port );
	areas.mailPrintf( "\n" );
    }
    areas.mailStop();

    startTime = clock();
}   // openAreasAndStartTimer



static void stopTimerEtc( int retcode ) /*fold00*/
{
    clock_t deltaTime;
    long deltaTimeS10;

    if (TSocket::getTotalBytesRcvd() + TSocket::getTotalBytesXmtd() != 0) {
	deltaTime = clock() - startTime;
	deltaTimeS10 = (10*deltaTime) / CLOCKS_PER_SEC;
	if (deltaTimeS10 == 0)
	    deltaTimeS10 = 1;
        areas.mailPrintf1( OW_NEWS,"\n" );
	areas.mailPrintf1( OW_NEWS,"totally %lu bytes received, %lu bytes transmitted\n",
			   TSocket::getTotalBytesRcvd(), TSocket::getTotalBytesXmtd() );
	areas.mailPrintf1( OW_NEWS,"%ld.%ds elapsed, throughput %ld bytes/s\n",
			   deltaTimeS10 / 10, deltaTimeS10 % 10,
			   (10*(TSocket::getTotalBytesRcvd()+TSocket::getTotalBytesXmtd())) / deltaTimeS10 );
    }

    areas.mailStart();
    areas.mailPrintf( "finished, retcode=%d\n", retcode );
    areas.mailStop();
}   // stopTimerEtc



static void usage( const char *fmt, ... ) /*fold00*/
//
//  display some usage information
//  usage() should be called, before the status mail has been generated (i.e. areas is
//  still closed).  That means too, that usage() cannot generate any stsmail.msg.
//
{
    if (fmt != NULL) {
	va_list ap;
	char buf[BUFSIZ];
	
	va_start( ap,fmt );
	vsprintfT( buf,fmt,ap );
	va_end( ap );
	hprintfT( STDERR_FILENO,"%s\n\n",buf );
    }

    hprintfT( STDERR_FILENO, "%s\ntask:  transfer POP3 mail and NNTP news to SOUP\n",VERSION );
    hprintfT( STDERR_FILENO, "usage: %s [options] [URLs]\n",progname );
    hputsT("  URL:  (nntp|pop3|smtp)://[userid[:password]@]host[:port]\n",STDERR_FILENO );
    hputsT("global options:\n",STDERR_FILENO );
    hputsT("  -h dir   Set home directory ('.' is legal)\n", STDERR_FILENO);
#ifdef OS2
    hputsT("  -i       Do not read 'Internet Connection for OS/2' settings\n", STDERR_FILENO);
#endif
    hputsT("  -m       Do not get mail\n", STDERR_FILENO);
    hputsT("  -M       Generate status mail\n", STDERR_FILENO);
    hputsT("  -n       Do not get news\n", STDERR_FILENO);
    hputsT("  -Q       Query some information from the servers (for reception only)\n", STDERR_FILENO);
    hputsT("  -r       Read only mode.  Do not delete mail or update newsrc\n", STDERR_FILENO);
    hputsT("  -s       Send replies\n", STDERR_FILENO);
    hputsT("  -T n     Limit for throughput surveillance [default: 0]\n", STDERR_FILENO);
    hputsT("  -Z       new handling of command line options: -m, -n, -s will now *enable*\n", STDERR_FILENO);
    hputsT("           the corresponding action\n", STDERR_FILENO );

    hputsT("news reading options:\n", STDERR_FILENO);
    hputsT("  -a       Add new newsgroups to newsrc file\n", STDERR_FILENO);
    hputsT("  -c[n]    Mark every article as read except for the last n [default: 10]\n", STDERR_FILENO);
    hputsT("  -C n     Set initial catchup count of newly subscibed groups\n", STDERR_FILENO);
    hputsT("  -k n     Set maximum news packet size in kBytes\n", STDERR_FILENO);
    hputsT("  -K file  Set score file\n", STDERR_FILENO);
    hputsT("  -N file  Set newsrc file\n", STDERR_FILENO);
#if defined(__MT__)
    hputsT("  -S n     News reading strategy [0..2, default 1]\n", STDERR_FILENO );
    hprintfT( STDERR_FILENO,"  -t n     Number of threads [1..%d, standard: %d]\n",
	      MAXNNTPTHREADS,DEFNNTPTHREADS );
#endif
    hputsT("  -u       Create news summary\n", STDERR_FILENO);
    hputsT("  -x       Do not process news Xref headers\n", STDERR_FILENO);
    hputsT("mail reading option:\n", STDERR_FILENO );
    hputsT("  -D       Force deletion of mail on POP3 server on each message\n", STDERR_FILENO );
    hputsT("  -Y       Do not use APOP even if available\n", STDERR_FILENO );
    hputsT("news transmission options (multiple destinations allowed):\n", STDERR_FILENO );
    hputsT("  -S n     News transmission strategy [1=standard (default), 0=push feed]\n", STDERR_FILENO );
    hputsT("mail transmission options:\n", STDERR_FILENO );
    hputsT("  -X       exclude CC/BCC from addressee list\n", STDERR_FILENO );

    assert( _heapchk() == _HEAPOK );

    exit( EXIT_FAILURE );
}   // usage



static void parseCmdLine( int argc, char **argv, int doAction ) /*FOLD00*/
{
    int c;
    int i;

    optind = 0;
    while ((c = getopt(argc, argv, "?ac::C:DE:h:iK:k:MmN:nQrRsS:t:T:uxXYZ")) != EOF) {
	if (!doAction) {
#ifdef OS2
	    if (c == 'i')
		doIni = 0;
#endif
	}
	else {
	    switch (c) {
		case '?':
		    usage( NULL );
		    break;
		case 'a':
		    doNewGroups = 1;
		    break;
		case 'c':
		    mode = CATCHUP;
		    if (optarg != NULL) {
			catchupCount = atoi(optarg);
			if (catchupCount < 0)
			    catchupCount = 0;
		    }
		    else
			catchupCount = 10;
		    break;
                case 'C':
                    initialCatchupCount = atol( optarg );
                    break;
		case 'D':
		    forceMailDelete = 1;
                    break;

                case 'E':
                    smtpEnvelopeOnlyDomain = optarg;       // undocumented feature!
                    break;
                    
		case 'h':
		    homeDir = optarg;
		    setFiles();
		    break;
#ifdef OS2
		case 'i':
		    break;
#endif
		case 'K':
		    strcpy(killFile, optarg);
		    killFileOption = 1;
		    break;
		case 'k':
		    maxBytes = atol(optarg) * 1000L;
		    break;
		case 'M':
		    areas.forceMail();
		    break;
		case 'm':
                    opt_m = 1;
		    break;
		case 'N':
		    strcpy(newsrcFile, optarg);
		    break;
		case 'n':
                    opt_n = 1;
		    break;
		case 'Q':
		    mode = QUERY;
		    break;
		case 'r':
		    readOnly = 1;
                    break;
                case 'R':
                    useReceived = 1;                       // undocumented feature!
                    break;
                case 's':
                    opt_s = 1;
		    break;
		case 'S':
		    newsStrategy = atoi(optarg);
		    break;
#if defined(__MT__)
		case 't':
		    maxNntpThreads = atoi(optarg);
		    if (maxNntpThreads < 1  ||  maxNntpThreads > MAXNNTPTHREADS)
			usage( "ill num of threads %s",optarg );
		    threadCntGiven = 1;
		    break;
#endif
		case 'T':
		    minThroughput = atol(optarg);
		    break;
		case 'u':
		    doNews = 1;
		    doSummary = 1;
		    break;
		case 'x':
		    doXref = 0;
                    break;
                case 'X':
                    includeCC = 0;
                    break;
                case 'Y':
                    doAPOP = 0;
                    break;
                case 'Z':
                    opt_Z = 1;
                    break;
		default:
		    usage( NULL );
	    }
	}
    }

    if ( !opt_Z) {
        //
        // old handling of command line options:
        // -m -> disable mail reception
        // -n -> disable news reception
        // -s -> do transmission
        // newsXmtStrategy can always be set
        //
        doMail = (opt_m == 0);
        doNews = (opt_n == 0);
        doXmt  = (opt_s != 0);
        if (opt_s)
            mode = SEND;
        newsXmtStrategy = newsStrategy;
    }
    else {
        //
        // new handling of command line options
        // -m -> enable mail reception
        // -n -> enable news reception
        // -s -> enable transmission
        // newsXmtStrategy can only be set, if no -n is specified, otherwise it reverts to 'standard'
        //
        doMail = (opt_m != 0);
        doNews = (opt_n != 0);
        doXmt  = (opt_s != 0);
        if ( !opt_n)
            newsXmtStrategy = newsStrategy;
        else
            newsXmtStrategy = 1;
    }
    
    //
    //  get the URLs
    //
    if (doAction) {
	for (i = optind;  i < argc;  i++) {
	    if (strnicmp("smtp://",argv[i],7) == 0)
		getUrlInfo( argv[i]+7, smtpInfo );
	    else if (strnicmp("pop3://",argv[i],7) == 0)
		getUrlInfo( argv[i]+7, pop3Info );
	    else if (strnicmp("nntp://",argv[i],7) == 0) {
		if (nntpXmtCnt >= ((mode == SEND) ? MAXNNTPXMTCNT : 1))
		    usage( "too many nntp:// destinations" );
		nntpInfo[nntpXmtCnt].host   = NULL;
		nntpInfo[nntpXmtCnt].user   = NULL;
		nntpInfo[nntpXmtCnt].passwd = NULL;
		nntpInfo[nntpXmtCnt].port   = -1;
		getUrlInfo( argv[i]+7, nntpInfo[nntpXmtCnt++] );
	    }
	    else
		usage( "ill URL %s",argv[i] );
	}
    }

    if (nntpXmtCnt == 0  &&  nntpInfo[0].host != NULL)
	nntpXmtCnt = 1;
}   // parseCmdLine



#ifdef OS2
static void readTcpIni (void) /*fold00*/
//
//  if a hostname is not specified, corresponding *Info remains unchanged, i.e. NULL (rg100398)
//
{
    HAB hab;
    HINI hini;
    char *etc;
    char buf[BUFSIZ];
    char curConnect[200];

    etc = getenv("ETC");
    if (etc == NULL) {
	hputsT( "Must set ETC\n", STDERR_FILENO );
	exit( EXIT_FAILURE );
    }
    sprintfT(buf, "%s\\TCPOS2.INI", etc);

    hab = WinInitialize(0);
    hini = PrfOpenProfile(hab, buf);
    if (hini == NULLHANDLE) {
	hprintfT( STDERR_FILENO, "cannot open profile %s\n", buf );
	exit(EXIT_FAILURE);
    }

    PrfQueryProfileString(hini, "CONNECTION", "CURRENT_CONNECTION", "",
                          curConnect, sizeof(curConnect));

    PrfQueryProfileString(hini, curConnect, "POPSRVR", "", buf, sizeof(buf));
    if (buf != NULL  &&  *buf != '\0')
	xstrdup( &pop3Info.host,buf );
    PrfQueryProfileString(hini, curConnect, "POP_ID", "", buf, sizeof(buf));
    xstrdup( &pop3Info.user,buf );
    xstrdup( &smtpInfo.user,buf );
    xstrdup( &nntpInfo[0].user,buf );
    PrfQueryProfileString(hini, curConnect, "POP_PWD", "", buf, sizeof(buf));
    xstrdup( &pop3Info.passwd,buf );
    xstrdup( &smtpInfo.passwd,buf );
    xstrdup( &nntpInfo[0].passwd,buf );    

    PrfQueryProfileString(hini, curConnect, "DEFAULT_NEWS", "", buf, sizeof(buf));
    if (buf != NULL  &&  *buf != '\0')
	xstrdup( &nntpInfo[0].host,buf );

    PrfQueryProfileString(hini, curConnect, "MAIL_GW", "", buf, sizeof(buf));
    if (buf != NULL  &&  *buf != '\0')
	xstrdup( &smtpInfo.host,buf );
    
    PrfCloseProfile(hini);
    WinTerminate(hab);

#ifdef DEBUG
    printfT( "TCPOS2.INI information:\n" );
    printfT( "-----------------------\n" );
    printfT( "nntpServer:   %s\n", nntpInfo[0].host );
    printfT( "popServer:    %s\n", pop3Info.host );
    printfT( "mailGateway:  %s\n", smtpInfo.host );
    printfT( "-----------------------\n" );
#endif
}   // readTcpIni
#endif



static void signalHandler( int signo ) /*fold00*/
//
//  Signal handling:
//  -  abort the sockets (sockets are unblocked...)
//  -  close the files
//  -  output an error message
//
//  es ist sehr die Frage, ob man hier sprintfT etc verwenden darf, da nicht 100%
//  klar ist, ob ein abgeschossener Thread die zugehîrigen Semaphore freigibt!? (scheint aber so...)
//
{
    //
    //  if SIGINT will be received, consecutive reception of the signal
    //  will be disabled.  Otherwise set handling to default
    //  (e.g. SIGTERM twice will terminate also signalHandler())
    //
    signal( signo, (signo == SIGUSR1  ||  signo == SIGINT) ? SIG_IGN : SIG_DFL );
    signal( signo, SIG_ACK );

#ifndef NDEBUG
    printfT( "\nmain thread received signal %d\n",signo );
#endif

    prepareAbort();

    //  durch's Abbrechen gibt es manchmal einen SIGSEGV der sub-threads, da sie
    //  z.B. auf eine Datei zugreifen, die schon geschlossen wurde.
    //  die Kinder mÅssen zuerst gekillt werden (aber wie??)
    //  Holzhammer:  die Threads brechen bei SIGSEGV ab.  Die Sockets werden auf
    //  NONBLOCK gestellt und lîsen ein SIGUSR1 aus, wenn abgebrochen werden soll
    //

    areas.mailException();          // otherwise semaphors could block forever
    
    areas.mailPrintf1( OW_NEWS,"\n" );
    areas.mailPrintf1( OW_NEWS,"*** signal %d received ***\n",signo );
    areas.mailPrintf1( OW_NEWS,"\n" );

    stopTimerEtc( EXIT_FAILURE );

    areas.forceMail();          // generate status mail in case of signal reception
    areas.closeAll();

    if ( !readOnly)
	newsrc.writeFile();

    _sleep2( 200 );

    exit( EXIT_FAILURE );       // wird ein raise() gemacht, so werden die exit-Routinen nicht aufgerufen (z.B. files lîschen)
}   // signalHandler



//--------------------------------------------------------------------------------------


static void threadMailReceive( void * ) /*FOLD00*/
{
    if (doMail) {
        OutputCR( OW_MAIL );
        if ( !getMail( pop3Info.host,pop3Info.user,pop3Info.passwd,pop3Info.port ))
            retcode = EXIT_FAILURE;
        OutputCR( OW_MAIL );
        Output( OW_MAIL, "-- mail reception completed\n" );
    }
    finishedMail = 1;
}   // threadMailReceive


static void threadNewsReceive( void * ) /*fold00*/
{
    if (doNews) {
        OutputCR( OW_NEWS );
        if (doSummary) {
            if ( !sumNews())
                retcode = EXIT_FAILURE;
        }
        else {
            if ( !getNews(newsStrategy))
                retcode = EXIT_FAILURE;
        }
        OutputCR( OW_NEWS );
        Output( OW_NEWS, "-- news reception completed\n" );
    }
    finishedNews = 1;
}   // threadNewsReceive


static void threadXmt( void * ) /*fold00*/
{
    if (doXmt) {
        OutputCR( OW_XMIT );
        if ( !sendReply(newsXmtStrategy))
            retcode = EXIT_FAILURE;
        OutputCR( OW_XMIT );
        Output( OW_XMIT, "-- transmission completed\n" );
    }
    finishedXmt = 1;
}   // threadXmt


static void threadNewsCatchup( void * ) /*fold00*/
{
    OutputCR( OW_NEWS );
    if ( !catchupNews(catchupCount))
        retcode = EXIT_FAILURE;
    OutputCR( OW_NEWS );
    Output( OW_NEWS, "-- catchup completed\n" );
    finishedNews = 1;
}   // threadNewsCatchup


static void threadMailQuery( void * ) /*FOLD00*/
{
    if (doMail) {
        int res;

        OutputCR( OW_MAIL );
        res = getMailStatus( pop3Info.host,pop3Info.user,pop3Info.passwd,pop3Info.port );
        if (res < 0)
            retcode += 3;
        else if (res == 0)
            ;   //retcode = 0;
        else
            retcode += 4;
        OutputCR( OW_MAIL );
        Output( OW_MAIL, "-- mail query completed\n" );
    }
    finishedMail = 1;
}   // threadMailQuery


static void threadNewsQuery( void * ) /*fold00*/
{
    if (doNews) {
        long res;

        OutputCR( OW_NEWS );
        res = getNewsStatus();
        if (res < 0)
            retcode += 9;
        else if (res > 0)
            retcode += 16;
        OutputCR( OW_NEWS );
        Output( OW_NEWS, "-- news query completed\n" );
    }
    finishedNews = 1;
}   // threadNewsQuery


//--------------------------------------------------------------------------------------



int main( int argc, char **argv ) /*FOLD00*/
{
#ifdef DEBUG
    setvbuf( stdout,NULL,_IONBF,0 );
#endif

#ifdef OS2
    if (_osmode != OS2_MODE) {
	hprintfT( STDERR_FILENO,"sorry, DOS not sufficient...\n" );
	exit( EXIT_FAILURE );
    }
#endif
    
    signal(SIGINT,   signalHandler );     // ^C
    signal(SIGBREAK, signalHandler );     // ^Break
    signal(SIGHUP,   signalHandler );     // hang up
    signal(SIGPIPE,  signalHandler );     // broken pipe
    signal(SIGTERM,  signalHandler );     // kill (lÑ·t der sich doch catchen?)
    signal(SIGUSR1,  SIG_IGN );           // ignore this signal in the main thread (which should not be aborted...)
	
    progname = strrchr(argv[0], '\\');
    if (progname == NULL)
	progname = argv[0];
    else
	++progname;

    //
    //  get some environment vars (HOME, NNTPSERVER)
    //
    if (argc < 2)
        usage( "This version requires at least one command line argument!" );
    parseCmdLine(argc, argv, 0);    // only get doIni (-i)
#ifdef OS2
    if (doIni)
	readTcpIni();
#endif
    if ((homeDir = getenv("HOME")) == NULL)
	homeDir = ".";
    if (getenv("NNTPSERVER") != NULL)
	getUrlInfo( getenv("NNTPSERVER"), nntpInfo[0] );
    setFiles();
    parseCmdLine(argc, argv, 1);
    
    assert( _heapchk() == _HEAPOK );

    openAreasAndStartTimer(argc,argv);

    //
    //  check the number of free file handles
    //
#ifndef HANDLEERR
    if (mode == RECEIVE  &&  doNews) {
	int h = nhandles(150);    // maximum 150 handles

#ifdef DEBUG_ALL
	printfT( "nhandles() returned %d\n", h );
#endif
	if (maxNntpThreads+8 > h) {
	    if (threadCntGiven)
		areas.mailPrintf1( OW_NEWS,"not enough file handles for %d connected threads\n",
				   maxNntpThreads );
	    maxNntpThreads = h-8;
	    if (threadCntGiven)
		areas.mailPrintf1( OW_NEWS,"number of threads cut to %d (increase the -h setting in the EMXOPT\n        environment variable in CONFIG.SYS, e.g. 'SET EMXOPT=-h40')\n",
				   maxNntpThreads );
	    if (maxNntpThreads < 1)
		exit( EXIT_FAILURE );
	}
    }
#endif

#if defined(__MT__)
    BEGINTHREAD( checkTransfer, NULL );
#endif

    OutputInit();
    Output( OW_MISC, VERSION );

    finishedMail = finishedNews = finishedXmt = 1;
    switch (mode) {
        case RECEIVE:
            OutputOpenWindows( (doMail ? (1 << OW_MAIL) : 0) + (doNews ? (1 << OW_NEWS) : 0) + (doXmt ? (1 << OW_XMIT) : 0),
                             NULL, NULL, NULL );
            finishedMail = finishedNews = finishedXmt = 0;
            BEGINTHREAD( threadMailReceive, NULL );
            BEGINTHREAD( threadNewsReceive, NULL );
            BEGINTHREAD( threadXmt, NULL );
	    break;
	    
        case SEND:
            finishedXmt = 0;
            OutputOpenWindows( 1 << OW_XMIT, NULL, NULL, NULL );
            BEGINTHREAD( threadXmt, NULL );
	    break;
	    
        case CATCHUP:
            finishedNews = 0;
            OutputOpenWindows( 1 << OW_NEWS, NULL, NULL, NULL );
            BEGINTHREAD( threadNewsCatchup, NULL );
	    break;

	case QUERY:
            OutputOpenWindows( (doMail ? (1 << OW_MAIL) : 0) + (doNews ? (1 << OW_NEWS) : 0),
                             NULL, NULL, NULL );
            retcode = 0;
            finishedMail = finishedNews = 0;
            BEGINTHREAD( threadMailQuery, NULL );
            BEGINTHREAD( threadNewsQuery, NULL );
            break;
    }

    //
    //  wait for termination of the several threads
    //
    while (!finishedNews  |  !finishedMail  |  !finishedXmt)
        _sleep2( 250 );
    
    stopTimerEtc( retcode );

    prepareAbort();

    if (retcode & EXIT_FAILURE)
	areas.forceMail();
    areas.closeAll();

    assert( _heapchk() == _HEAPOK );

    OutputExit();
    
    exit( retcode );
}   // main
