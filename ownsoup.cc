//  $Id: ownsoup.cc 1.6 1999/08/29 13:12:30 Hardy Exp Hardy $
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
//  Get the messages with a specific pattern in them and put them into a specific
//  newsgroup.
//
//  input:       areas, *.msg
//  output:      modified areas, *.msg, extra .msg
//  parameters:  <pattern> <groupname> <outfile>
//
//  - the input *.msg must be in binary newsgroup format ("B") or USENET format ("u")
//  - matching articles are appended to <outfile> in binary mail format "bn"
//  - the first line of the found article will be "X-ownsoup: <groupname>"
//  - if outfile is found in areas, it is not scanned again...
//  - .MSG is added to outfile
//  - upper/lower case is ignored
//  - 'u' files are read in text mode, 'B' files in binary
//
//  To catch the articles you have to setup a filter, which matches
//  the "X-ownsoup" header
//



#include <getopt.h>
#include <regexp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nls.h>



#define AREAS     "./areas"
#define MSGEXT    ".msg"
#define XHEADER   "X-ownsoup: "



static const char *progname;
static const char *outName;
static const char *groupName;
static FILE *outF = NULL;

static int scanHeader  = 1;
static int scanBody    = 1;
static int shutUp      = 0;
static int somethingScanned = 0;



static int scanArticle( FILE *msgF, long msgLen, const regexp *pattern )
{
    long startpos = ftell(msgF);
    long endpos = startpos + msgLen;
    char line[BUFSIZ];
    int  match;
    long len;
    int  inHeader = 1;

    somethingScanned = 1;
    match = 0;
    len = msgLen;
    while (len > 0) {
	if (fgets( line,sizeof(line),msgF ) == NULL)
	    break;
	len -= strlen(line);

	if (*line == '\n')
	    inHeader = 0;

	if ((inHeader && scanHeader)  ||  (!inHeader && scanBody)  ||
	    (inHeader && scanBody && strncmp(line,"Subject",7) == 0)) {
	    _nls_strlwr( (unsigned char *)line );
	    match = regexec( pattern,line );
	    if (match)
		break;
	}
    }

    if (match) {
	unsigned char len1[4];
	char name[BUFSIZ];

	if (outF == NULL) {
	    sprintf( name,"%s%s",outName,MSGEXT );
	    outF = fopen( name,"ab" );     // append!
	}
	sprintf( name,"%s%s\n",XHEADER,groupName );

	len = msgLen + strlen(name );
	len1[3] = (len >>  0) & 0xff;
	len1[2] = (len >>  8) & 0xff;
	len1[1] = (len >> 16) & 0xff;
	len1[0] = (len >> 24) & 0xff;
	fwrite( len1,sizeof(len1),1, outF );
	fputs( name,outF );

	fseek( msgF,startpos,SEEK_SET );
	while (msgLen > 0) {
	    char buf[4096];
	    size_t get;

	    get = ((unsigned)msgLen > sizeof(buf)) ? sizeof(buf) : msgLen;
	    if (fread(buf,1,get,msgF) != get) {
		perror( "fread" );
		exit( EXIT_FAILURE );
	    }
	    if (fwrite(buf,1,get,outF) != get) {
		perror( "fwrite" );
		exit( EXIT_FAILURE );
	    }
	    msgLen -= get;
	}
    }
    
    fseek( msgF, endpos,SEEK_SET );
    return match;
}   // scanArticle



static void usage( void )
{
    printf( "\n%s v0.26 (rg190197)\n\tgenerate mail file from news according to <regexp>\n\n", progname );
    printf( "usage:  %s [OPTION] <regexp> <groupname> <outputfile>\n",progname );
    printf( "  -b   scan article body only (subject is part of body)\n" );
    printf( "  -h   scan article header\n" );
    printf( "  -q   be (almost) quiet\n" );
    exit( EXIT_FAILURE );
}   // usage



int main( int argc, char *argv[] )
//
//  principal algo
//  - get command line parameters
//  - open areas file
//  - while not eof(areas)
//  -    read line, identify type, filename
//  -    if type ok, open file
//  -       for each article in file, search for pattern
//  -       if pattern contained, write article to output
//  -    end if
//  - end while
//  - if there was an output article, then file to areas
//
//  Questions:  is it required to change the msgId? (hopefully not...)
//
{
    char buf[BUFSIZ];
    FILE *areasF, *msgF;
    char fname[BUFSIZ], gname[BUFSIZ], stype[BUFSIZ];
    char mname[BUFSIZ];
    unsigned char len1[4];
    long msgLen;
    regexp *pattern;
    int matches = 0;
    int totmatch = 0;
    int outfilefound = 0;
    int c;

    progname = strrchr(argv[0], '\\');
    if (progname == NULL)
	progname = argv[0];
    else
	++progname;

    while ((c = getopt(argc, argv, "?bhq")) != EOF) {
            switch (c) {
		case '?':
		    usage();
		    break;
		case 'q':
		    shutUp = 1;
		    break;
		case 'h':
		    scanHeader  = 1;
		    scanBody    = 0;
		    break;
		case 'b':
		    scanBody    = 1;
		    scanHeader  = 0;
		    break;
		default:
		    printf( "%s: ill option -%c\n", progname,c );
		    usage();
		    break;
	    }
    }
	    
    if (argc-optind != 3) {
	printf( "%s: not enough parameters %d %d\n",progname,optind, argc );
	usage();
    }

    _nls_strlwr( (unsigned char *)argv[optind] );              // is this legal??
    pattern = regcomp( argv[optind] );
    groupName = argv[optind+1];
    outName = argv[optind+2];
    
    areasF = fopen( AREAS,"rt" );
    if (areasF == NULL) {
	printf( "%s: %s not found\n", progname, AREAS );
	exit( EXIT_FAILURE );
    }

    while (fgets(buf,sizeof(buf),areasF) != NULL) {
	matches = 0;
	*fname = *gname = *stype = '\0';
	sscanf( buf,"%[^\t]\t%[^\t]\t%[^\t]%*s", fname,gname,stype );
	if (stricmp(fname,outName) == 0) {
	    if ( !shutUp)
		printf( "%s: %s%s skipped\n", progname,fname,MSGEXT );
	    outfilefound = 1;
	}
	else if (*stype == 'B') {
	    if ( !shutUp)
		printf( "%s: %s in %s%s binary news format\n", progname, gname, fname, MSGEXT );
	    sprintf( mname,"%s%s", fname,MSGEXT );
	    msgF = fopen( mname,"rb" );
	    if (msgF != NULL) {
		while (fread(len1, sizeof(len1),1, msgF) == 1) {
		    msgLen = (len1[0] << 24) +
			(len1[1] << 16) +
			(len1[2] <<  8) +
			(len1[3] <<  0);
		    if (scanArticle( msgF,msgLen,pattern ))
			++matches;
		}
		fclose( msgF );
	    }
	}
	else if (*stype == 'u') {
	    if ( !shutUp)
		printf( "%s: %s in %s%s USENET news format\n", progname, gname, fname, MSGEXT );
	    sprintf( mname,"%s%s", fname,MSGEXT );
	    msgF = fopen( mname,"rt" );
	    if (msgF != NULL) {
		char line[100];
		while (fgets(line,sizeof(line),msgF) != NULL) {
		    sscanf( line,"%*s%ld",&msgLen );
		    if (scanArticle( msgF,msgLen,pattern ))
			++matches;
		}
		fclose( msgF );
	    }
	}
	if (matches != 0) {
	    if ( !shutUp)
		printf( "%s: %d matches\n", progname,matches );
	    totmatch += matches;
	}
    }
    fclose( areasF );
    
    if (outF != NULL  &&  !outfilefound) {
	if ( !shutUp)
	    printf( "%s: %s%s created\n",progname,outName,MSGEXT );

	areasF = fopen( AREAS,"ab" );
	fprintf( areasF,"%s\t%s\tbn\n", outName,groupName );
	fclose( areasF );
    }
    {
	char type[100];

	strcpy( type,"" );
	if (scanHeader)
	    strcat( type,"header" );
	if (scanBody) {
	    if (*type != '\0')
		strcat( type,"/" );
	    strcat( type,"body&subject" );
	}
	if (somethingScanned  ||  !shutUp)
	    printf( "%s: %d match%s of \"%s\" found in %s\n",progname,totmatch,
		    (totmatch != 1) ? "es" : "", argv[optind], type );
    }
    if ( !shutUp)
	printf( "%s: setup filter for \"%s%s\"\n", progname,XHEADER,groupName );

    exit( EXIT_SUCCESS );
}   // main
