//  $Id: nntpcl.cc 1.32 1999/06/13 16:40:44 Hardy Exp Hardy $
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


#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rgfile.hh>
#include <rgmts.hh>
#include <rgsocket.hh>

#include "global.hh"    // only for XVER*
#include "nntp.hh"
#include "nntpcl.hh"
#include "util.hh"



//
//  is this a misfeature of GCC, is there a better way to do it??
//
#define STR2(x) #x
#define STR(x)  STR2(x)



static TSemaphor cntSema;     // static in class tut nicht (gcc2.7.0)



//--------------------------------------------------------------------------------



TNntp::TNntp( void )
{
#ifdef TRACE_ALL
    printfT( "TNntp::TNntp()\n" );
#endif
    xrefHook  = NULL;
    killQHook = NULL;
    actGroup = xstrdup("");
    selGroup = xstrdup("");
    user     = xstrdup("");
    passwd   = xstrdup("");
    strcpy( lastErrMsg, "unknown error condition" );
    artNotAvail = 0;
}   // TNntp::TNntp



TNntp::~TNntp()
{
#ifdef TRACE_ALL
    printfT( "TNntp::~TNntp()\n" );
#endif
    close( 0 );
////    delete actGroup;
////    delete selGroup;
    //// delete user;
    //// delete passwd;
}   // TNntp::~TNntp



void TNntp::setHelper( void (*xref)(const char *xrefLine),
		       long (*killQ)(const char *groupName, const char *headerLine ) )
{
    xrefHook  = xref;
    killQHook = killQ;
}   // TNntp::setHelper



TNntp::Res TNntp::request( const char *cmd, char *reply, size_t replySize,
			   int expReply )
//
//  Send a request to NNTP server and check the result (cmd must not end with \n)
//  If the server request authentication, the AUTHINFO procedure according to
//  RFC977-extension will be executed.
//  If the coonection has been broken, then *reply == '\0'
//
{
    int retcode;
    int loopCnt;

#ifdef TRACE
    printfT("%02d: TNntp::request(%s,,,%d)\n",sock,cmd,expReply );
#endif

    *reply = '\0';
    loopCnt = 0;
    for (;;) {
	//
	//  three retries for the command
	//
	if (loopCnt++ >= 3) {
	    strcpy( lastErrMsg,"nntp server is in a loop requesting AUTHINFO..." );
	    return nok;
	}

	//
	//  transmit the command & fetch the result
	//
	if (printf( "%s\n",cmd ) < 0) {
	    sprintfT( lastErrMsg,"%s:  cannot transmit", cmd );
	    *reply = '\0';
	    return nok;
	}
	if (gets(reply,replySize) == NULL) {
	    sprintfT( lastErrMsg,"%s:  no reply", cmd );
	    *reply = '\0';
	    return nok;
	}
#ifdef DEBUG
	printfT( "%02d: TNntp::request(): %s\n",sock,reply );
#endif
	if (reply[0] == CHAR_FATAL) {
	    sprintfT( lastErrMsg,"%s:  fatal (%s)", cmd,reply );
	    return nok;
	}

	//
	//  if return code != ERR_NOAUTH, we are done (-> check the result)
	//
	retcode = atoi(reply);
	if (retcode != ERR_NOAUTH)
	    break;

#ifdef DEBUG_ALL
	hprintfT( STDERR_FILENO,"authentication requested\n" );
	printfT( "authentication requested\n" );
#endif
	//
	//  otherwise do the authentication
	//
	printf( "AUTHINFO USER %s\n",user );
	if (gets(reply,replySize) == NULL) {
	    strcpy( lastErrMsg,"AUTHINFO USER:  no reply" );
	    *reply = '\0';
	    return nok;
	}
	retcode = atoi(reply);
	if (retcode == OK_AUTH)
	    continue;
	if (retcode != NEED_AUTHDATA) {
	    sprintfT( lastErrMsg,"AUTHINFO USER:  %s",reply );
	    return nok;
	}
	
	printf( "AUTHINFO PASS %s\n",passwd );
	if (gets(reply,replySize) == NULL) {
	    strcpy( lastErrMsg,"AUTHINFO PASS:  no reply" );
	    *reply = '\0';
	    return nok;
	}
	retcode = atoi(reply);
	if (retcode != OK_AUTH) {
	    sprintfT( lastErrMsg,"AUTHINFO PASS:  %s",reply );
	    return nok;
	}
#ifdef DEBUG_ALL
	hprintfT( STDERR_FILENO,"authentication ok\n" );
	printfT( "authentication ok\n" );
#endif
    }

    if (retcode != expReply) {
	sprintfT( lastErrMsg,"%s:  %s", cmd,reply );
	return nok;
    }
    return ok;
}   // TNntp::request



TNntp::Res TNntp::open( const char *nntpServer, const char *nntpUser,
			const char *nntpPasswd, int nntpPort, int openXmt )
//
//  Opens connection to news server.  If the socket is already open, the connection
//  will be re-opened!
//
//  ret=nok  ->  TSocket will be closed
//
{
    char buf[500];
    int  response;

#ifdef TRACE_ALL
    printfT( "TNntp::open(%s,,%d)\n",nntpServer,nntpPort );
#endif
    readOnly = 0;
    xstrdup(   &user,nntpUser );
    xstrdup( &passwd,nntpPasswd );

    if (nntpServer == NULL  ||  *nntpServer == '\0') {
	strcpy( lastErrMsg,"no news server defined" );
	return nok;
    }

    if (TSocket::open( nntpServer,"nntp","tcp",nntpPort ) < 0) {
	strcpy( lastErrMsg,"cannot open socket" );
	return nok;
    }

    if (gets(buf, sizeof(buf)) == NULL) {
	strcpy( lastErrMsg,"connect:  no reply" );
#ifdef DEBUG
	printfT( "TNntp::open():  socket: %s\n",buf );
#endif
	TSocket::close();
	return nok;
    }
    else {
	response = atoi(buf);
	switch (response) {

	case OK_NOPOST:
	    readOnly = 1;
	    if (openXmt) {
		sprintfT( lastErrMsg,"connect:  no permission to post, %s",buf );
		TSocket::close();
		return nok;
	    }
	    break;
	    
	case OK_CANPOST:
	    break;
	    
	case ERR_ACCESS:
	    sprintfT( lastErrMsg,"connect:  no permission, %s",buf );
	    TSocket::close();
	    return nok;
	    
	default:
	    sprintfT( lastErrMsg,"connect:  ill response, %s",buf );
#ifdef DEBUG
	    printfT( "TNntp::open():  illresp: %s\n",buf );
#endif
	    TSocket::close();
	    return nok;
	}
    }

    //
    //  Don't send the 'mode reader' for transmissions.
    //  This was introduced for pushing news to InterNotes which will
    //  hang on unexpected commands...
    //
    if ( !openXmt) {
	//
	// This is for INN (result is ignored)
	//
	char buf2[500];

	request( "MODE READER",buf2,sizeof(buf2),OK_CANPOST );
#ifdef DEBUG_ALL
	printfT( "TNntp::open():  antwort auf mode reader: %s\n",buf2 );
#endif
    }

    sprintfT( lastErrMsg,"%s",buf );

    //
    //  create temporary file
    //
    if ( !tmpF.isOpen()) {
	if ( !tmpF.open()) {
	    strcpy( lastErrMsg,"create of temporary file failed" );
	    TSocket::close();
	    return nok;
	}
    }
#ifdef DEBUG_ALL
    printfT( "TNntp::open(): connected\n" );
#endif
    return ok;
}   // TNntnp::open



void TNntp::close( int sendQuit )
{
#ifdef TRACE_ALL
    printfT( "TNntp::close(%d)\n",sendQuit );
#endif
    if (sendQuit) {
	char buf[100];

#ifdef TRACE_ALL
	printfT( "TNntp::close(): QUIT\n" );
#endif
	request( "QUIT",buf,sizeof(buf), OK_GOODBYE );
    }
    TSocket::close();
    tmpF.close();
}   // TNntp::close



const char *TNntp::getLastErrMsg( void )
{
    return lastErrMsg;
}   // TNntp::getLastErrMsg



TNntp::Res TNntp::getXhdr( const char *headerField, long first, long last,
			   int (*callback)(int operation, const char *line) )
//
//  Get the XHDR 'lines' (shortest)
//  callback()-fct is called for each received line (example in news.cc)
//  Correct group must be selected for this thread
//
{
    char buf[200];
    char cmd[100];
    Res  res;

#ifdef TRACE_ALL
    printfT( "getXhdr(%s,%ld,%ld)\n",headerField,first,last );
#endif

    sprintfT( cmd,"XHDR %s %ld-%ld",headerField,first,last );
    if (request(cmd,buf,sizeof(buf),OK_HEAD) != ok) {
#ifdef DEBUG_ALL
	printfT( "getXhdr-error: %s\n",lastErrMsg );
#endif
	return ok;     // no problem!
    }

    sprintfT( buf,"%ld",first );    // init callback
    callback( 1,buf );
    callback( 2,actGroup );

    res = nok;
    while (gets(buf, sizeof(buf)) != NULL) {
#ifdef TRACE_ALL
	printfT( "xhdr: %s\n",buf );
#endif
	if (buf[0] == '.') {
	    res = ok;
	    break;
	}
	if ( !callback(0,buf))
	    break;
    }
    return res;
}   // TNntp::getXhdr



TNntp::Res TNntp::getNewGroups( const char *nntpTimeFile, int changeFile )
//
//  fetch new groups to file
//
{
    char oldTime[80], nntpTime[80], buf[NNTP_STRLEN];
    TFile dateF;
    int  getall;
    char *p;
    Res  res;

#ifdef TRACE
    printfT( "getNewGroups()\n" );
#endif

    //
    //  get current date/time from NNTP server
    //
    if (request("DATE",buf,sizeof(buf),INF_DATE) == ok)
	sscanfT( buf+4, "%s", nntpTime );
    else {
	time_t now = time(NULL);
	strftime( nntpTime, sizeof(nntpTime), "%Y%m%d%H%M%S", gmtime(&now) );
    }
	
    //
    //  Get last date/time we checked for new newsgroups.
    //
    getall = 0;
    *oldTime = '\0';
    if (dateF.open(nntpTimeFile,TFile::mread,TFile::otext)) {
	dateF.fgets( oldTime, sizeof(oldTime), 1 );
	dateF.close();
    }
    //
    //  check time stamp (no file is also caught)
    //
    {
	int i;
	for (i = 0;  i < 14;  ++i)
	    getall = getall  ||  !isdigit(oldTime[i]);
    }

    //
    //  Request new newsgroups.
    //
    {
	char cmd[100];

	if (getall)
	    strcpy( cmd,"LIST" );
	else
	    sprintfT( cmd,"NEWGROUPS %-6.6s %-6.6s GMT", oldTime+2, oldTime+8);
	if (request(cmd,buf,sizeof(buf),getall ? OK_GROUPS : OK_NEWGROUPS) != ok)
	    return nok;
    }

    tmpF.truncate( 0L );
    res = nok;
    while (gets(buf, sizeof(buf)) != NULL) {
#ifdef DEBUG
	printfT( "rcv: %s\n",buf );
#endif
	if (buf[0] == '.') {
	    res = ok;
	    break;
	}
	if ((p = strchr(buf, ' ')) != NULL)
	    *p = '\0';

	tmpF.printf( "%s\n",buf );
    }

    //
    //  Save current date/time.
    //
    if (changeFile) {
	if (dateF.open(nntpTimeFile,TFile::mwrite,TFile::otext,1)) {
	    dateF.printf( "%s\n",nntpTime );
	    dateF.close();
	}
    }
    if (res != ok)
	strcpy( lastErrMsg,"LIST/NEWGROUPS aborted" );
#ifdef TRACE
    printfT( "TNntp::getNewGroups(): finished\n" );
#endif
    return res;
}   // TNntp::getNewGroups



TNntp::Res TNntp::getOverview( long first, long last )
//
//  Attention:  those overview lines are sometimes VERY long (references...)
//
{
    char buf[NNTP_STRLEN];
    char cmd[100];
    Res  res;

#ifdef TRACE_ALL
    printfT( "TNntp::getOverview(%ld,%ld,%s)\n",first,last,selGroup );
#endif
    tmpF.truncate( 0L );

    if (first < last)
	sprintfT( cmd,"XOVER %ld-%ld", first, last );
    else {
	if (first == 0)
	    return ok;
	sprintfT( cmd,"XOVER %ld-", first );
    }
    if (request(cmd,buf,sizeof(buf),OK_XOVER) != ok)
	return nok;

    res = nok;
    while (gets(buf, sizeof(buf)) != NULL) {
	if (buf[0] == '.') {
	    res = ok;
	    break;
	}
	tmpF.printf( "%s\n",buf );
#ifdef TRACE_ALL
//	printfT( "%s\n",buf );
#endif
    }
    if (res != ok)
	strcpy( lastErrMsg,"XOVER aborted" );
    return res;
}   // TNntp::getOverview



TNntp::Res TNntp::setActGroup( const char *group, long &cnt, long &lo, long &hi )
//
//  activate nntp group
//  returns:  ok,nok,notavail
//
{
    char buf[NNTP_STRLEN];
    char cmd[100];
    long l1,l2,l3;

#ifdef TRACE_ALL
    printfT( "TNntp::setActGroup(%s,..)\n",group );
#endif

    xstrdup( &actGroup,group );
    selNntpArticle = -1;

    sprintfT( cmd,"GROUP %s",group );
    if (request(cmd,buf,sizeof(buf),OK_GROUP) != ok) {
	xstrdup( &actGroup,"" );
	return (buf[0] == '\0') ? nok : notavail;
    }

    sscanfT(buf+4, "%ld %ld %ld", &l1, &l2, &l3);
    cnt = l1;  lo = l2;  hi = l3;
////	selNntpArticle = l2;  w„re korrekt, bringt es aber nicht so fr den NEXT
    nntpArtHi    = l3;
    nntpArtFirst = l2;
    
#ifdef TRACE_ALL
    printfT( "TNntp::setActGroup(%s,%ld,%ld,%ld)\n",group,cnt,lo,hi );
#endif
    return ok;
}   // TNntp::setActGroup



TNntp::Res TNntp::nextArticle( long *next )
//
//  Get next article in group.
//  Return ok if successful.
//
{
    char buf[NNTP_STRLEN];

    if (request("NEXT",buf,sizeof(buf),OK_NOTEXT) != ok) {
	*next = selNntpArticle = nntpArtHi;
	return nok;                            // no next article
    }
    *next = selNntpArticle = atol(buf+4);

#ifdef DEBUG_ALL
    printfT( "nntpNext() -> %ld\n",*next );
#endif
    return ok;
}   // TNntp::nextArticle



void TNntp::selectArticle( const char *grpname, long artNum, int doKill,
			   long artFirst, long artHi )
{
#ifdef TRACE_ALL
    printfT( "selectArticle(%s,%ld,%d,%ld,%ld)\n",
	     grpname,artNum,doKill,artFirst,artHi );
#endif

    if (grpname != NULL) {
	if (strcmp(selGroup,grpname) != 0)
	    xstrdup( &selGroup,grpname );
    }

    selArticle  = artNum;
    killEnabled = doKill;
    if (artFirst > 0)
	nntpArtFirst = artFirst;
    if (artHi > 0)
	nntpArtHi = artHi;
}   // TNntp::selectArticle



TNntp::Res TNntp::_getHead( void )
//
//  Get the articles header and write it to a temporary file (tmpF)
//  killing & cross referencing is handled here
//  return:  ok,nok,killed,notavail
//
{
    char buf[NNTP_STRLEN];
    char cmd[100];
    char gotXref;
    Res  res;
    long articleScore;
    long killThreshold;
    int  maxScore;            // actually the lowest score (most significant more killing the article)...

#ifdef TRACE_ALL
    printfT( "_getHead(): %ld\n",selArticle );
#endif

    //
    //  request article (head)
    //
    sprintfT( cmd,"%s %ld", killEnabled ? "HEAD" : "ARTICLE",selArticle );
    if (request(cmd,buf,sizeof(buf),killEnabled ? OK_HEAD : OK_ARTICLE) != ok)
	return (buf[0] == CHAR_ERR) ? notavail : nok;
    selNntpArticle = selArticle;

    articleScore = 0;
    gotXref = 0;
    killThreshold = 0;
    if (killEnabled  &&  killQHook != NULL)
	killThreshold = killQHook( NULL, NULL );    // hack: killQHook return killthreshold
    maxScore = 0;

    //
    //  Get lines of article head.
    //
    res = nok;
    while (gets(buf, sizeof(buf)) != NULL) {
	char *bufp = buf;

#ifdef DEBUG_ALL
	printfT( "--1: %ld '%s'\n",selArticle,bufp );
#endif

	if (killEnabled) {
	    if (buf[0] == '.')
		if (*(++bufp) == '\0') {
		    res = ok;
		    break;
		}
	}
	else if (*bufp == '\0') {
	    res = ok;
	    break;
	}
	
	tmpF.printf( "%s\n",bufp);

	if (killEnabled  &&  killQHook != NULL) {
            long lineScore = killQHook(selGroup,bufp);
            articleScore  += lineScore;
            if (lineScore < maxScore) {
		strcpy( lastErrMsg, bufp );       // put line with maxScore to errMsg
                maxScore = lineScore;
	    }
	}

	if (xrefHook != NULL  &&  !gotXref  &&  strnicmp(bufp, "xref: ", 6) == 0) {
	    xrefHook(bufp+6);
	    gotXref = 1;           // why is only one Xref allowed ?
	}
    }

    //
    //  Don't process anymore if article was killed.
    //
    if (articleScore < killThreshold) {
	assert( killEnabled );
	return killed;
    }

    //
    //  Put empty line separating head from body.
    //
    tmpF.putcc('\n');
    if (res != ok)
	strcpy( lastErrMsg,"HEAD/ARTICLE aborted" );
    return res;
}   // TNntp::_getHead



TNntp::Res TNntp::_getBody( void )
//
//  Get the articles body and write it to a temporary file (tmpF)
//  should not be called, if article is going to be killed
//  return:  nok, ok
//
{
    char buf[NNTP_STRLEN];
    Res  res;

#ifdef TRACE_ALL
    printfT( "_getBody(): %ld\n",selArticle );
#endif
    
    if (killEnabled) {
	char cmd[100];

	sprintfT( cmd,"BODY %ld", selArticle );
	if (request(cmd,buf,sizeof(buf),OK_BODY) != ok)
	    return (buf[0] == CHAR_ERR) ? notavail : nok;
	selNntpArticle = selArticle;
    }

    //
    //  Retrieve article body.
    //
    res = nok;
    while (gets(buf, sizeof(buf)) != NULL) {
	char *bufp = buf;

	if (buf[0] == '.') {
	    if (*(++bufp) == '\0') {
		res = ok;               // -> end of article !
		break;
	    }
	}
	tmpF.printf( "%s\n",bufp );
#ifdef DEBUG_ALL
	printfT( "--2: %ld '%s'\n",selArticle,bufp );
#endif
    }
    if (res != ok)
	strcpy( lastErrMsg,"BODY/ARTICLE aborted" );
    return res;
}   // _getBody



TNntp::Res TNntp::getArticle( void )
//
//  Get the article and write it to a temporary file (tmpF)
//  killing & cross referencing is handled here
//  return:  ok,nok,notvail,killed
//  calls: _getHead, _getBody
//
{
    Res res;

#ifdef TRACE_ALL
    printfT( "getArticle(): %ld\n",selArticle );
#endif

#ifdef TRACE_ALL
    printfT( "--0: %ld\n",selArticle );
#endif

    //
    //  select the group, if required
    //
    if (strcmp(actGroup,selGroup) != 0) {
	long d0,d1,d2;
	res = setActGroup( selGroup, d0,d1,d2 );
	if (res != ok)
	    return nok;   // ignore notavail in this case!
    }

#ifdef TRACE_ALL
    printfT( "--1: %ld\n",selArticle );
#endif

    //
    //  Get article to temporary file.
    //
    tmpF.seek(0L, SEEK_SET);

    res = _getHead();
    if (res != ok)
	return res;

#ifdef TRACE_ALL
    printfT( "--2: %ld\n",selArticle );
#endif

    res = _getBody();

#ifdef TRACE_ALL
    printfT( "--3: %ld\n",selArticle );
#endif
    return res;
}   // TNntp::getArticle



TNntp::Res TNntp::postArticle( TFile &file, size_t bytes )
//
//  Post article to NNTP server.
//  on entry:  filehandle points to beginning of message
//             'bytes' contains message size
//  on exit:   filehandle points to end of message
//  Return ok if successful, nok if it's not possible to continue, nok_goon
//  if caller may continue
//
{
    char buf[NNTP_STRLEN];
    size_t count;
    long offset;
    int  sol;                 // start of line
    int  ll;                  // line length
    int  inHeader;
    int  xverUAfound;

#ifdef TRACE
    printfT( "TNntp::postArticle(.,%ld)\n",bytes );
#endif
    //
    //  if there is a negative reply for the POST command, then we are
    //  not allowed to post to this server at all (at least at the moment) -> fatal problem
    //
    if (request("POST",buf,sizeof(buf),CONT_POST) != ok)
	return nok;

    offset = file.tell();
    count = bytes;
    sol = 1;
    inHeader = 1;
    xverUAfound = 0;
    while (file.fgets(buf,sizeof(buf)) != NULL  &&  count > 0) {
	//
	//  - replace trailing "\r\n" with "\n"
	//  - send the string to the socket
	//  - set sol, countdown artlength
	//
	ll  = strlen(buf);
	if (strcmp( buf+ll-2,"\r\n" ) == 0)
            strcpy( buf+ll-2,"\n" );
        if (inHeader  &&  sol) {
            if (isHeader(buf,XVER_UA0))
                xverUAfound = 1;
            else if (buf[0] == '\n') {
                inHeader = 0;
#if defined(XVER_UA0)
                printf( "%s\n", !xverUAfound ? XVER_UA : XVER_NR );
#endif
            }
        }
	printf( "%s%s", (sol && buf[0] == '.') ? "." : "", buf );
	sol = (buf[strlen(buf)-1] == '\n');
	count -= ll;
    }
    file.seek(offset+bytes, SEEK_SET);

    if (request(".",buf,sizeof(buf),OK_POSTED) == ok)
	return ok;
    
    if (atoi(buf) == ERR_POSTFAIL)
	sprintfT( lastErrMsg, "POST:  article not accepted by server; not posted\n        (%s)",buf );
    else
	sprintfT( lastErrMsg, "POST:  %s",buf );

    //
    //  if the server replied with a 'dont resend' or so,
    //  an ok-condition is faked
    //
    if (strstr(buf,STR(ERR_GOTIT))    != NULL  ||
	strstr(buf,STR(ERR_XFERRJCT)) != NULL)
	return ok;

    //
    //  if there was no reply, then a fatal problem occured
    //
    if (*buf == '\0')
	return nok;
    return nok_goon;
}   // TNntp::postArticle



TNntp::Res TNntp::ihaveArticle( TFile &file, size_t bytes, const char *msgId )
//
//  Post article to NNTP server via IHAVE.
//  on entry:  filehandle points to beginning of message
//             'bytes' contains message size
//             'msgId' is the message ID extracted from the articles header
//  on exit:   filehandle points to end of message
//  Return ok if successful, nok if it's not possible to continue, nok_goon
//  if caller may continue
//
{
    char buf[NNTP_STRLEN];
    char cmd[NNTP_STRLEN];
    size_t count;
    long offset;
    int  sol;                 // start of line
    int  ll;                  // line length
    int  inHeader;
    int  xverUAfound;

#ifdef TRACE
    printfT( "TNntp::ihaveArticle(.,%ld,%s)\n",bytes,msgId );
#endif

    //
    //  IHAVE will result in "335" (continue) or "435" (gotit).
    //  otherwise we have a fatal problem (do not continue transfer)
    //
    sprintfT( cmd,"IHAVE %s",msgId );
    if (request(cmd,buf,sizeof(buf),CONT_XFER) != ok) {
	file.seek(bytes,SEEK_CUR);
	if (atoi(buf) == ERR_GOTIT)
	    return ok;
	return nok;
    }

    offset = file.tell();
    count = bytes;
    sol = 1;
    inHeader = 1;
    xverUAfound = 0;
    while (file.fgets(buf,sizeof(buf)) != NULL  &&  count > 0) {
	//
	//  - replace trailing "\r\n" with "\n"
	//  - send the string to the socket
	//  - set sol, countdown artlength
	//
	ll  = strlen(buf);
	if (strcmp( buf+ll-2,"\r\n" ) == 0)
	    strcpy( buf+ll-2,"\n" );
        if (inHeader  &&  sol) {
            if (isHeader(buf,XVER_UA0))
                xverUAfound = 1;
            else if (buf[0] == '\n') {
                inHeader = 0;
#if defined(XVER_UA0)
                printf( "%s\n", !xverUAfound ? XVER_UA : XVER_NR );
#endif
            }
        }
	printf( "%s%s", (sol && buf[0] == '.') ? "." : "", buf );
	sol = (buf[strlen(buf)-1] == '\n');
	count -= ll;
    }
    file.seek(offset+bytes, SEEK_SET);

    //
    // end of article
    //
    if (request(".",buf,sizeof(buf),OK_XFERED) == ok)
	return ok;

    //
    //  if there was no reply, then a fatal problem occured
    //
    if (*buf == '\0')
	return nok;
    return nok_goon;
}   // TNntp::ihaveArticle
