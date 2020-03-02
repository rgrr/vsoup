//  $Id: qsoup.cc 1.5 1999/08/29 13:14:31 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//
//  This is a very simple simulation of inews for VSoup
//
//  Algorithm is as follows:
//  ------------------------
//  for (;;) {
//     wait until no replies-file exists in that subdir
//     lock %yarn%\history.pag (write access)
//     recheck replies
//     if (ok) 
//        break
//     unlock history.pag
//  }
//  unzip -oq reply.zip
//  if (I have got news article)
//     add news.msg to replies (if not already there)
//     append the temporary to news.msg (know USENET & binary format)
//  else
//     add mail.msg to replies (if not already there)
//     append the temporary to mail.msg (know USENET & binary format)
//  delete temporary
//  zip -0m reply.zip replies *.msg
//  unlock history.pag
//
//  Attention:
//  ----------
//  -  lines of the written message are delimited by '\n' only
//  -  this is a hack with almost no subroutines
//
//  
//  Fixed settings:
//  ---------------
//  - call of unzip (unzip.exe -oq)
//  - call of zip (zip -0mq)
//  - name of history.pag
//  - name of yarn config file (%HOME%\yarn\config)
//  - name of reply-packet field in yarn config file
//  - name of the SOUP replies file
//
//  Parameters:
//  -----------
//  -m        input file is a mail
//  -v        be verbose
//  -l<file>  lockfile, default is %YARN%\history.pag
//  -r<file>  set the name of the reply packet, default is to check %HOME%\yarn\config
//  -i        ignore rest of command line
//  <file>    one input file (if left out, stdin will be used)
//



#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <getopt.h>
#include <share.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>



#define ENVHOME          "HOME"
#define ENVYARN          "YARN"
#define YARNLOCKFILE     "history.pag"
#define YARNCONFIGFILE   "yarn\\config"
#define REPLYPACKETFIELD "reply-packet"
#define AREANEWS         "news"
#define AREAMAIL         "mail"
#define FILENEWS         "NEWS"
#define FILEMAIL         "MAIL"
#define SOUPREPLIES      "replies"
#define SOUPMSG          "*.msg"
#define SOUPEXT          ".msg"
#define CMDUNZIP         "unzip -oq"
#define CMDZIP           "zip -0mq"

#define CMDSENDMAIL      "sendmail"
#define CMDINEWS         "inews"



static char  progname[FILENAME_MAX];
static char  *homedir   = NULL;
static char  *yarndir   = NULL;
static int   doNews     = 1;
static int   verbose    = 0;
static FILE  *inputF    = NULL;
static FILE  *tmpInputF = NULL;

static int   lockF      = -1;

static char lockFile[FILENAME_MAX];
static char replyPacket[FILENAME_MAX];
static char replyDir[FILENAME_MAX];
static char startCwd[FILENAME_MAX];

static char fitFile[FILENAME_MAX];
static char fitType;



static int fileExists( const char *s )
{
    int h;
    
    h = open( s, O_BINARY | O_RDONLY );
    if (h == -1)
	return 0;
    close( h );
    return 1;
}   // fileExists



static int existReplies( void )
{
    char name[FILENAME_MAX];

    sprintf( name,"%s\\%s",replyDir,SOUPREPLIES );
    if ( !fileExists(name)) {
	if (verbose)
	    printf( "(%s:ok)",name );
	return 0;
    }
    if (verbose)
	printf( "(%s:denied)",name );
    return 1;
}   // existReplies



static int lockYarn( void )
{
    assert( lockF == -1 );

    lockF = sopen( lockFile, O_WRONLY | O_BINARY, SH_DENYRW );
    if (verbose)
	printf( "(%s:%s)",lockFile, (lockF == -1) ? "denied" : "ok" );
    return lockF != -1;
}   // lockYarn



static void unlockYarn( void )
{
    assert( lockF != -1 );

    close( lockF );
    lockF = -1;
}   // unlockYarn



static void usage( void )
{
    printf( "\n%s v0.10 (rg071196)\n\tcopy input into the yarn reply-packet (simulates inews)\n\n", progname );
    printf( "usage:  %s [OPTION] [<inputfile>]\n",progname );
    printf( "  -m        the input is a mail (default news)\n" );
    printf( "  -v        be verbose\n" );
    printf( "  -l<file>  specify lockfile, default %%%s%%\\%s\n",ENVYARN,YARNLOCKFILE );
    printf( "  -r<file>  specify name of reply packet, default is to get it\n\t    from %%HOME%%\\%s\n",YARNCONFIGFILE );
    printf( "  -i        ignore remainder of command line\n" );
    printf( "if <inputfile> is omitted stdin will be used\n" );
    exit( EXIT_FAILURE );
}   // usage



int main( int argc, char *argv[] )
{
    int c;
    FILE *tmpf;
    char line[512];

    {
	char *p1 = strrchr( argv[0],'/' );
	char *p2 = strrchr( argv[0],'\\' );
	if (p1 == NULL  &&  p2 == NULL)
	    strcpy( progname,argv[0] );
	else
	    strcpy( progname, (p1 > p2) ? p1+1 : p2+1 );
	if ((p1 = strrchr(progname,'.')) != NULL)
	    *p1 = '\0';
    }

    homedir = getenv( ENVHOME );
    yarndir = getenv( ENVYARN );

    inputF = stdin;
    _fsetmode( inputF,"t" );

    if (stricmp(progname,CMDSENDMAIL) == NULL) {
	doNews = 0;
	verbose = 0;
    }
    else if (stricmp(progname,CMDINEWS) == NULL) {
	doNews = 1;
	verbose = 0;
    }
    else {
	while ((c = getopt(argc, argv, "?mvr:l:i")) != EOF) {
            switch (c) {
		case '?':
		    usage();
		    break;
		case 'm':
		    doNews = 0;
		    break;
		case 'v':
		    verbose = 1;
		    break;
		case 'r':
		    strcpy( replyPacket,optarg );
		    break;
		case 'l':
		    strcpy( lockFile,optarg );
		    break;
		case 'i':
		    goto IgnoreCmdLine;
		default:
		    printf( "%s: ill option -%c\n", progname,c );
		    usage();
		    break;
	    }
	}
	if (argc-optind > 1) {
	    printf( "\n%s: too many args\n", progname );
	    usage();
	}

	if (argc-optind == 1) {
	    if (verbose)
		printf( "%s: input file %s\n",progname,argv[optind] );
	    inputF = fopen( argv[optind],"rt" );
	    if (inputF == NULL) {
		printf( "%s: cannot open %s\n",progname,argv[optind] );
		exit( EXIT_FAILURE );
	    }
	}
    }

IgnoreCmdLine:
    if (homedir == NULL  &&  *replyPacket == '\0') {
	printf( "%s: %%%s%% or -r<file> required\n", progname,ENVHOME );
	exit( EXIT_FAILURE );
    }

    if (yarndir == NULL  &&  *lockFile == '\0') {
	printf( "%s: %%%s%% or -l<file> required\n", progname,ENVYARN );
	exit( EXIT_FAILURE );
    }

    if (*lockFile == '\0')
	sprintf( lockFile, "%s\\%s",yarndir,YARNLOCKFILE );
    if (verbose)
	printf( "%s: using %s as lockfile\n",progname,lockFile );

    if (*replyPacket == '\0') {
	char yarnConfFile[FILENAME_MAX];

	//
	//
	//  get reply-packet line from yarn config file
	//
	sprintf( yarnConfFile, "%s\\%s",homedir,YARNCONFIGFILE );
	if (verbose)
	    printf( "%s: using %s as yarn config file\n",progname,yarnConfFile );

	tmpf = fopen( yarnConfFile,"rt" );
	if (tmpf == NULL) {
	    printf( "%s: cannot open %s\n",progname,yarnConfFile );
	    exit( EXIT_FAILURE );
	}

	while (fgets(line,sizeof(line),tmpf) != NULL) {
	    char tag[512];
	
	    *replyPacket = *tag = '\0';
	    sscanf( line,"%[^=]=%s",tag,replyPacket );
	    if (stricmp(tag,REPLYPACKETFIELD) == 0  &&  *replyPacket != '\0')
		break;
	    *replyPacket = '\0';
	}
	fclose( tmpf );

	if (*replyPacket == '\0') {
	    printf( "%s: %s configuration in %s required\n",progname,REPLYPACKETFIELD,yarnConfFile );
	    exit( EXIT_FAILURE );
	}

	if (verbose)
	    printf( "%s: %s=%s\n",progname,REPLYPACKETFIELD,replyPacket );
    }
    if (verbose)
	printf( "%s: using %s as reply packet\n",progname,replyPacket );


    //
    //  get directory of reply-packet
    //
    {
	char *p1, *p2;

	strcpy( replyDir,replyPacket );
	p1 = strrchr( replyDir,'/' );
	p2 = strrchr( replyDir,'\\' );
	if (p1 == NULL  &&  p2 == NULL) {
	    printf( "%s: ill definition of %s\n",progname,REPLYPACKETFIELD );
	    exit( EXIT_FAILURE );
	}
	if (p1 > p2)
	    *p1 = '\0';
	else
	    *p2 = '\0';
    }
    if (verbose)
	printf( "%s: directory of %s: %s\n",progname,REPLYPACKETFIELD,replyDir );


    //
    //  set working directory to replyDir
    //  (before tmpfile() is called!  Advantage: the tempfiles are in the replyDir)
    //
    _getcwd2( startCwd,sizeof(startCwd) );
    if (verbose)
	printf( "%s: current working dir %s\n", progname,startCwd );
    if (_chdir2(replyDir) == -1) {
	printf( "%s: cannot change to %s\n", progname,replyDir );
	exit( EXIT_FAILURE );
    }
    if (verbose)
	printf( "%s: directory changed to %s\n", progname,replyDir );


    //
    //  read the input file and copy it to tmpInputF
    //  Advantage:  tmpInputF lines are delimited by '\n' only!!!
    //
    tmpInputF = tmpfile();
    if (verbose)
	printf( "%s: copying input to tmpfile\n",progname );

    {
	int c;
	int sol = 1;
	while ((c = fgetc(inputF)) != EOF  &&  c != 0x1a) {  // check for 0x1a, because sendmail requires it!
	    fputc( c,tmpInputF );
	    if (verbose) {
		if (sol)
		    printf( "%s: ",progname );
		putchar( c );
		sol = (c == '\n');
	    }
	}
    }
    fseek( tmpInputF, 0,SEEK_SET );


    //
    //  wait for access
    //
    if (verbose)
	printf( "%s: waiting for access to %s\n", progname,replyPacket );
    for (;;) {
	int noReplies, lockOk;

	noReplies = lockOk = 0;
	if ((noReplies = !existReplies())) {
	    if ((lockOk = lockYarn())) {
		if (verbose)
		    printf( "\r" );
		if ((noReplies = !existReplies()))
		    break;
		unlockYarn();
	    }
	}
	if ( !verbose) {
	    if ( !noReplies)
		printf( "\ranother instance accessing %s", replyPacket );
	    else if ( !lockOk)
		printf( "\ranother instance holding %s", lockFile );
	}
	sleep( 4 );
	printf( "\r%79s\r","" );
	sleep( 1 );
    }
    if (verbose) {
	printf( "\r%79s\r","" );
	printf( "%s: got access, %s locked\n", progname,lockFile );
    }


    //
    //  do the unzip
    //
    if (verbose)
	printf( "%s: unzipping\n", progname );
    if (fileExists(replyPacket)) {
	for (;;) {
	    char cmd[512];

	    sprintf( cmd,"%s %s",CMDUNZIP,replyPacket );
	    if (system(cmd) == 0)
		break;
	    if (verbose)
		printf( "\r%s failed",cmd );
	    sleep( 4 );
	    if (verbose)
		printf( "\r%79s\r","" );
	    sleep( 1 );
	}
	if (verbose)
	    printf( "%s: %s unzipped\n", progname,replyPacket );
    }
    else {
	if (verbose)
	    printf( "%s: no %s found (ok)\n", progname,replyPacket );
    }


    //
    //  search for best fit in SOUPREPLIES
    //  Yarn/Vsoup dependent
    //
    *fitFile = '\0';
    tmpf = fopen( SOUPREPLIES,"rt" );
    if (tmpf != NULL) {
	while (fgets(line,sizeof(line),tmpf) != NULL) {
	    char file[512],area[512],type[512];
	    *type = '\0';
	    sscanf( line,"%[^\t]\t%[^\t]\t%[^\t]",file,area,type );
	    if (doNews) {
		if (*type == 'B'  &&
		    (stricmp(file,FILENEWS) == NULL  ||  *fitFile == '\0')) {
		    strcpy( fitFile,file );
		    fitType = *type;
		}
		else if (*type == 'u'  &&  stricmp(area,AREANEWS) == 0) {
		    if (stricmp(file,FILENEWS) == NULL  ||  *fitFile == '\0') {
			strcpy( fitFile,file );
			fitType = *type;
		    }
		}
	    }
	    else {
		if (*type == 'b'  &&
		    (stricmp(file,FILEMAIL) == NULL  ||  *fitFile == '\0')) {
		    strcpy( fitFile,file );
		    fitType = *type;
		}
		else if (*type == 'u'  &&  stricmp(area,AREAMAIL) == 0) {
		    if (stricmp(file,FILEMAIL) == NULL  ||  *fitFile == '\0') {
			strcpy( fitFile,file );
			fitType = *type;
		    }
		}
	    }
	}
	fclose( tmpf );
	if (verbose  &&  *fitFile != '\0')
	    printf( "%s: fitting file is %s%s, type %c\n", progname,fitFile,SOUPEXT,fitType );
    }


    //
    //  of there was no fit, then setup default and append it to REPLIES
    //
    if (*fitFile == '\0') {
	int fcnt = 0;

	strcpy( fitFile, doNews ? FILENEWS : FILEMAIL );
	fitType = doNews ? 'B' : 'b';
	for (;;) {
	    char msgname[FILENAME_MAX];

	    sprintf( msgname,"%s%s",fitFile,SOUPEXT );
	    if ( !fileExists(msgname))
		break;
	    ++fcnt;
	    sprintf( fitFile,"%s%d", doNews ? FILENEWS : FILEMAIL, fcnt );
	}

	tmpf = fopen( SOUPREPLIES,"ab" );
	if (tmpf == NULL) {
	    printf( "%s: cannot create %s\n", progname, SOUPREPLIES );
	    exit( EXIT_FAILURE );
	}
	fprintf( tmpf,"%s\t%s\t%c\n", fitFile, (doNews) ? AREANEWS : AREAMAIL, fitType );
	fclose( tmpf );
	if (verbose)
	    printf( "%s: %s, type %c appended to %s\n",progname,fitFile,fitType,SOUPREPLIES );
    }


    //
    //  copy the temporary input file to the destination SOUP file
    //  first write length of packet to destination (depending on fitType)
    //
    {
	char msgname[FILENAME_MAX];
	long tmpSize;

	sprintf( msgname,"%s%s",fitFile,SOUPEXT );
	tmpf = fopen( msgname,"ab" );
	if (tmpf == NULL) {
	    printf( "%s:  cannot append to %s\n", progname,msgname );
	    exit( EXIT_FAILURE );
	}

	fseek( tmpInputF,0,SEEK_END );
	tmpSize = ftell( tmpInputF );
	fseek( tmpInputF,0,SEEK_SET );
	if (verbose)
	    printf( "%s: length of input=%ld\n",progname,tmpSize );
	if (fitType == 'u')
	    fprintf( tmpf,"#! rnews %ld\n",tmpSize );
	else {
	    unsigned char size[4];
	    size[0] = (tmpSize >> 24) & 0xff;
	    size[1] = (tmpSize >> 16) & 0xff;
	    size[2] = (tmpSize >>  8) & 0xff;
	    size[3] = (tmpSize >>  0) & 0xff;
	    fwrite( size,sizeof(size),1,tmpf );
	}
	for (;;) {
	    char buf[4096];
	    size_t got;
	    got = fread( buf,1,sizeof(buf),tmpInputF );
	    if (got == 0)
		break;
	    fwrite( buf,1,got,tmpf );
	}
	if (verbose)
	    printf( "%s: input copied to %s%s\n", progname,fitFile,SOUPEXT );
	fclose( tmpf );
    }


    //
    //  do the zipping (kann nur was werden, wenn REPLIES existiert)
    //
    if (verbose)
	printf( "%s: zipping\n",progname );
    if ( fileExists(SOUPREPLIES)) {
	for (;;) {
	    char cmd[512];
	    
	    sprintf( cmd,"%s %s %s %s",CMDZIP,replyPacket,SOUPMSG,SOUPREPLIES );
	    if (system(cmd) == 0)
		break;
	}
	if (verbose)
	    printf( "%s: %s & %s zipped into %s\n", progname,SOUPREPLIES,SOUPMSG,replyPacket );
    }
    else {
	if (verbose)
	    printf( "%s: %s does not exist, nothing zipped & nothing done\n", progname,SOUPREPLIES );
    }
    

#if 0    
    //
    //  go back to cwd
    //
    _chdir2( startCwd );
    if (verbose)
	printf( "%s: directory changed to %s\n", progname,startCwd );
#endif

    exit( EXIT_SUCCESS );
}   // main
