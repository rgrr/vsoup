//  $Id: convsoup.cc 1.6 1999/08/29 12:59:59 Hardy Exp Hardy $
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
//  Small programm to convert SOUP formats from "bn"/"Bn" to "mn"/"un"
//
//  The program must be started in the directory contining the areas file
//  and the corresponding msg files
//



#include <stdio.h>
#include <string.h>



#define AREAS      "AREAS"
#define AREASTMP   "AREAS.TMP"
#define MSGTMP     "msg.tmp"
#define MSGPOSTFIX ".MSG"



const char *progname;



static void openMsgs( const char *prefix, FILE **msg, FILE **msgTmp )
{
    char msgName[200];
    
    sprintf( msgName,"%s%s",prefix,MSGPOSTFIX );
    *msg = fopen( msgName,"rb" );
    if (*msg == NULL)
	fprintf( stderr,"%s: cannot open %s\n", progname,msgName );

    *msgTmp = fopen( MSGTMP,"wb" );
    if (*msgTmp == NULL)
	fprintf( stderr,"%s: cannot create %s\n", progname,MSGTMP );
}   // openMsgs



static void closeMsgs( const char *prefix, FILE *msg, FILE *msgTmp )
{
    if (msg != NULL  &&  msgTmp != NULL) {
	char msgName[200];

	sprintf( msgName,"%s%s",prefix,MSGPOSTFIX );
	fclose( msg );
	if (remove(msgName) == -1) {
	    fprintf( stderr,"%s:  cannot delete %s\n",progname,msgName );
	    exit( 3 );
	}
	fclose( msgTmp );
	rename( MSGTMP, msgName );
    }
    else {
	if (msg)
	    fclose( msg );
	if (msgTmp) {
	    fclose( msgTmp );
	    remove( MSGTMP );
	}
    }
}   // closeMsgs



int main( int, char *argv[] )
{
    FILE *areas, *areasTmp, *msg, *msgTmp;
    char buf[200];
    int res;
    char prefix[100],areaname[100],encoding[100],description[100];

    progname = strrchr(argv[0], '\\');
    if (progname == NULL)
	progname = argv[0];
    else
	++progname;

    fprintf( stderr,"%s - v0.02 rg230197\n", progname );
    fprintf( stderr,"%s: convert binary SOUP (b/B) to mailbox (m) and USENET (u) format\n", progname );

    areas = fopen( AREAS,"rb" );
    if (areas == NULL) {
	fprintf( stderr,"%s: %s does not exist\n", progname,AREAS );
	exit( 3 );
    }

    areasTmp = fopen( AREASTMP,"wb" );
    if (areasTmp == NULL) {
	fprintf( stderr,"%s: cannot create %s\n", progname,AREASTMP );
	exit( 3 );
    }

    while (fgets(buf,sizeof(buf),areas) != NULL) {
	*description = '\0';
	res = sscanf( buf,"%s%s%s%[^\n]",prefix,areaname,encoding,description );
	if (res < 3) {
	    fprintf( stderr,"%s: ill line in areas: %s", progname,buf );
	    fputs( buf,areasTmp );
	}
	else {
	    fprintf( stderr,"%s: %8s%s %3s: ",progname,prefix,MSGPOSTFIX,encoding );
	    
	    if (*encoding == 'b') {
		//
		//  8-bit binary mail to UNIX mailbox
		//
		fprintf( stderr,"8-bit binary mail to UNIX mailbox\n" );
		openMsgs( prefix, &msg, &msgTmp );
		if (msg != NULL  &&  msgTmp != NULL) {
		    for (;;) {
			unsigned char lenbuf[4];
			size_t artSize;
			
			if (fread( lenbuf,sizeof(lenbuf),1,msg ) != 1)
			    break;
			artSize = (lenbuf[0] << 24) +
			    (lenbuf[1] << 16) +
			    (lenbuf[2] <<  8) +
			    (lenbuf[3] <<  0);
			fprintf( msgTmp,"From ConvSoup Wed Oct 23 09:15 GMT 1996\n" );   // dummy
			while (artSize > 0) {
			    char line[BUFSIZ];
			    
			    if (fgets( line, (artSize < sizeof(line)) ? artSize+1 : sizeof(line),msg ) == NULL) {
				perror( "gets()" );
				exit( 3 );
			    }
			    artSize -= strlen( line );
			    if (strncmp(line,"From ",5) == 0)
				fputc( '>',msgTmp );
			    fputs( line,msgTmp );
			}
		    }
		    *encoding = 'm';
		}
		closeMsgs( prefix, msg, msgTmp );
	    }
	    else if (*encoding == 'B') {
		//
		//  8-bit binary news to UNIX mailbox
		//
		fprintf( stderr,"8-bit binary news to USENET news\n" );
		openMsgs( prefix, &msg, &msgTmp );
		if (msg != NULL  &&  msgTmp != NULL) {
		    for (;;) {
			unsigned char lenbuf[4];
			size_t artSize;
			
			if (fread( lenbuf,sizeof(lenbuf),1,msg ) != 1)
			    break;
			artSize = (lenbuf[0] << 24) +
			    (lenbuf[1] << 16) +
			    (lenbuf[2] <<  8) +
			    (lenbuf[3] <<  0);
			fprintf( msgTmp,"#! rnews %lu\n", artSize );
			while (artSize > 0) {
			    char buffer[4096];
			    size_t rd;

			    rd = fread( buffer,1,(artSize < sizeof(buffer)) ? artSize : sizeof(buffer),msg );
			    if (rd == 0) {
				perror( "fread()" );
				exit( 3 );
			    }
			    artSize -= rd;
			    fwrite( buffer,1,rd, msgTmp );
			}
		    }
		    *encoding = 'u';
		}
		closeMsgs( prefix, msg, msgTmp );
	    }
	    else
		fprintf( stderr,"skipped\n" );
	    
	    fprintf( areasTmp,"%s\t%s\t%s", prefix,areaname,encoding );
	    if (*description)
		fprintf( areasTmp,"\t%s",description );
	    fprintf( areasTmp,"\n" );
	}
    }

    fclose( areas );
    fclose( areasTmp );
    if (remove(AREAS) == -1) {
	fprintf( stderr,"%s:  cannot delete %s\n",progname,AREAS );
	exit( 3 );
    }
    rename( AREASTMP, AREAS );
}   // main
