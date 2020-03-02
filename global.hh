//  $Id: global.hh 1.19 1999/08/29 12:52:50 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __GLOBAL_HH__
#define __GLOBAL_HH__


#define VER0    "VSoup v1.2.9.47Beta"
#if defined(OS2)
#define VER1    VER0 " [OS/2]"
#elif defined(__WIN32__)
#define VER1    VER0 " [95/NT]"
#else
#define VER1    VER0 " [-unknown-]"
#endif
#define VERSION VER1 " (rg101299)"
#define XVER_UA0 "User-Agent"
#define XVER_UA  XVER_UA0 ": " VER1
#define XVER_NR  "X-Posting-Agent: " VER1


#ifdef DEFGLOBALS
#define EXTERN
#define INIT(i) = i
#else
#define EXTERN extern
#define INIT(i)
#endif


#include "areas.hh"
#include "newsrc.hh"


//
//  defines for threads
//
#if defined(__MT__)
#define MAXNNTPTHREADS 20                // bei mehr als 15 hat es zu wenig Files !?
#define MAXNNTPXMTCNT  4
#else
#define MAXNNTPTHREADS 1
#endif


#define TIMEOUT        (100L)            // timeout value for connecting, aborting etc.


//
//  global variables
//
#if MAXNNTPTHREADS < 4
#define DEFNNTPTHREADS MAXNNTPTHREADS
#else
#define DEFNNTPTHREADS 4
#endif

EXTERN int maxNntpThreads INIT(DEFNNTPTHREADS);

#ifdef DEFGLOBALS
   TAreasMail areas("AREAS","%07d");
   TNewsrc newsrc;
#else
   extern TAreasMail areas;
   extern TNewsrc newsrc;
#endif

EXTERN int doAbortProgram             INIT(0);


//
//  program options
//
EXTERN char doNewGroups               INIT(0);
EXTERN char doXref                    INIT(1);
EXTERN char *smtpEnvelopeOnlyDomain   INIT( NULL );
EXTERN char useReceived               INIT(0);
EXTERN char *homeDir                  INIT( NULL );
EXTERN char killFile[FILENAME_MAX]    INIT("");
EXTERN char killFileOption            INIT(0);
EXTERN unsigned long maxBytes         INIT(0);
EXTERN char newsrcFile[FILENAME_MAX]  INIT("");
EXTERN char *progname                 INIT(NULL);
EXTERN char readOnly                  INIT(0);
EXTERN char forceMailDelete           INIT(0);
EXTERN char doAPOP                    INIT(1);
EXTERN char includeCC                 INIT(1);
EXTERN long initialCatchupCount       INIT(-1);


//
//  host/user/passwd storage for the different protocols
//
struct serverInfo {
    const char *host;
    const char *user;
    const char *passwd;
    int port;
};

#ifdef DEFGLOBALS
serverInfo smtpInfo = {NULL,NULL,NULL,-1};
serverInfo pop3Info = {NULL,NULL,NULL,-1};
serverInfo nntpInfo[MAXNNTPXMTCNT] = {{NULL,NULL,NULL,-1}};   // only first elements needs to be initialized
int nntpXmtCnt = 0;
#else
extern serverInfo smtpInfo;
extern serverInfo pop3Info;
extern serverInfo nntpInfo[MAXNNTPXMTCNT];
extern int nntpXmtCnt;
#endif


//
//  filenames
//
#if defined(OS2)  ||  defined(__WIN32__)
#define FN_NEWSTIME "newstime"
#else
#define FN_NEWSTIME ".newstime"
#endif
#define FN_REPLIES  "replies"


#endif   // __GLOBAL_HH__
