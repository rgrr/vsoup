//  $Id: rgfile.cc 1.1 1998/04/12 16:25:01 hardy Exp $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@ibm.net.
//
//  This file is part of soup++ for OS/2.  Soup++ including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rgfile.hh"
#include "rgmts.hh"
#include "rgsema.hh"


//--------------------------------------------------------------------------------



TFileTmp::TFileTmp( void )
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::TFileTmp()\n" );
#endif
    memBuff     = NULL;
    memBuffSize = 0;
}   // TFileTmp::TFileTmp



TFileTmp::~TFileTmp()
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::~TFileTmp()\n" );
#endif
    close();
}   // TFileTmp::~TFileTmp



void TFileTmp::close( void )
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::close() %s\n",fname );
#endif
    if (memBuff != NULL) {
	memBuffSize = 0;
	delete memBuff;
	memBuff = NULL;
    }
}   // TFileTmp::close



int TFileTmp::open( int increment )
//
//  ein tempor„rer File ist immer bin„r und ist immer read/write!
//
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::open(%d)\n", increment );
#endif
    filePos = 0;
    fileLen = 0;
    memBuff          = new unsigned char[ increment ];
    memBuffIncrement = increment;
    memBuffSize      = increment;

    TFile::open( NULL ,TFile::mreadwrite, obinary,1);

    xstrdup( &fname, "temp file" );
    return 1;
}   // TFileTmp::open



int TFileTmp::iread( void *buff, int bufflen )
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::iread(%p,%d) %s\n",buff,bufflen,fname );
#endif

    assert( mode == mread  ||  mode == mreadwrite );
    //
    //  compare remaining file data to requested size bufflen
    //
    if (bufflen >= 0  &&  filePos + bufflen > fileLen)
	bufflen = fileLen - filePos;

    if (bufflen > 0)
	memcpy( buff, memBuff+filePos, bufflen );

    return bufflen;
}   // TFileTmp::iread



int TFileTmp::iwrite( const void *buff, int bufflen )
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::iwrite(%p,%d) %s\n",buff,bufflen,fname );
#endif

    assert( mode == mwrite  ||  mode == mreadwrite );
    if (bufflen <= 0)
	return bufflen;
    
    if (filePos+bufflen >= memBuffSize) {
	memBuffSize = memBuffIncrement * ((filePos+bufflen) / memBuffIncrement + 1);
	memBuff = (unsigned char *)realloc( memBuff, memBuffSize );
#ifdef DEBUG_ALL
	printfT( "TFileTmp::iwrite(): memBuffSize=%d\n",memBuffSize );
#endif
    }
    memcpy( memBuff+filePos, buff, bufflen );

    return bufflen;
}   // TFileTmp::iwrite



int TFileTmp::truncate( long offset )
//
//  filePos is undefined after truncate()
//
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::truncate(%ld)\n",offset );
#endif
    if (offset < fileLen)
	fileLen = offset;
    if (filePos > fileLen)
	filePos = fileLen;
    return 0;
}   // TFileTmp::truncate



long TFileTmp::iseek( long offset, int origin )
{
#ifdef TRACE_ALL
    printfT( "TFileTmp::iseek(%ld,%d) %s\n",offset,origin,fname );
#endif

    //
    //  calculate new file position
    //
    if (origin == SEEK_SET)
	filePos = offset;
    else if (origin == SEEK_CUR)
	filePos += offset;
    else if (origin == SEEK_END)
	filePos = fileLen + offset;
    if (filePos < 0)
	filePos = 0;
    else if (filePos > fileLen)
	filePos = fileLen;
    return filePos;
}   // TFileTmp::iseek



//--------------------------------------------------------------------------------



TFile::TFile( void )
{
#ifdef TRACE_ALL
    printfT( "TFile::TFile()\n" );
#endif
    mode  = mnotopen;
    fname = NULL;
    filePos = 0;
    fileLen = 0;
}   // TFile::TFile



TFile::~TFile()
{
#ifdef TRACE_ALL
    printfT( "TFile::~TFile()\n" );
#endif
    close();
}   // TFile::~TFile



void TFile::close( int killif0, int forceKill )
{
#ifdef TRACE_ALL
    printfT( "TFile::close(%d,%d) %s\n",killif0,forceKill,fname );
#endif

    if (mode != mnotopen) {
        int kill = (killif0  &&  (::tell(handle) == 0))  ||  forceKill;
	::closeT( handle );
	if (kill)
	    ::removeT( fname );
    }

    if (fname != NULL) {
	delete fname;
	fname = NULL;
    }
    if (mode == mread  ||  mode == mreadwrite) {
	delete buffer;
	buffer = NULL;
	buffSize = 0;
	invalidateBuffer();
    }
    mode   = mnotopen;
    handle = -1;
}   // TFile::close



void TFile::remove( void )
{
    close( 0,1 );
}   // TFile::remove



int TFile::open( const char *name, TMode mode, OMode textmode, int create )
//
//  in case of mwrite, the file pointer is positioned at end of file
//  returns 0, if not successful
//  if name==NULL, then no file will be generated!
//
{
    int flag;

#ifdef TRACE_ALL
    printfT( "TFile::open(%s,%d,%d,%d)\n",name,mode,textmode,create );
#endif

    if (TFile::mode != mnotopen)
	close(0);

    if (name != NULL) {
	if (textmode == otext)
	    flag = O_TEXT;
	else
	    flag = O_BINARY;
	
	if (mode == mread)
	    flag |= O_RDONLY;
	else if (mode == mwrite)
	    flag |= O_WRONLY;
	else if (mode == mreadwrite)
	    flag |= O_RDWR;
	else {
	    errno = EPERM;
	    return 0;
	}
    
	if (create)
	    flag |= O_CREAT | O_TRUNC;

	handle = ::openT( name, flag );
	if (handle < 0)
	    return 0;
    }
    else
	handle = 11111;

    seek( 0,SEEK_END );           // set fileLen
    fileLen = filePos;
    if (mode != mwrite)
	seek( 0,SEEK_SET );       // sets also filePos
    
    TFile::mode = mode;
    xstrdup( &fname, name );
    buffSize = 0;
    buffer = NULL;
    if (mode == mread  ||  mode == mreadwrite) {
	buffSize = 128;
	buffer = new unsigned char[buffSize+10];
    }
    invalidateBuffer();
    return 1;
}   // TFile::open



int TFile::truncate( long length )
{
    int res = ::ftruncateT( handle, length );
    seek( 0, SEEK_END );
    return res;
}   // TFile::truncate



long TFile::seek( long offset, int origin )
{
#ifdef TRACE_ALL
    printfT( "TFile::seek(%ld,%d)\n",offset,origin );
#endif
    invalidateBuffer();
    return iseek( offset, origin );
}   // TFile::seek



long TFile::iseek( long offset, int origin )
{
    long pos;
    
    pos = ::lseekT( handle,offset,origin );
    if (pos >= 0)
	filePos = pos;
    return pos;
}   // TFile::iseek



long TFile::tell( void )
{
    return filePos;
}   // TFile::tell



int TFile::write( const void *buff, int bufflen )
{
    int r;

    if (bufflen <= 0)
	return 0;

    //
    //  set the physical filePos correctly (fillBuff() problem)
    //
    if (buffNdx < buffEnd)
	seek( filePos,SEEK_SET );   // also invalidates Buffer
    
    r = iwrite( buff,bufflen );
    if (r > 0) {
	filePos += r;
	if (filePos > fileLen)
	    fileLen = filePos;
    }
    return r;
}   // TFile::write



int TFile::iwrite( const void *buff, int bufflen )
//
//  must not change filePos!
//
{
    assert( mode == mwrite  ||  mode == mreadwrite );
    return ::writeT( handle,buff,bufflen );
}   // TFile::iwrite



int TFile::putcc( char c )
{
    return (write(&c,1) == 1) ? 1 : EOF;
}   // TFile::putc



int TFile::fputs( const char *s )
{
    int len, res;

    len = strlen(s);
    res = write( s,len );
    return (res == len) ? len : EOF;
}   // TFile::fputs



int TFile::printf( const char *fmt, ... )
{
    va_list ap;
    char buf[BUFSIZ];
    int  buflen;

    va_start( ap, fmt );
    buflen = ::vsprintfT( buf, fmt, ap );
    va_end( ap );
    return write( buf,buflen );
}   // TFile::printf



int TFile::vprintf( const char *fmt, va_list arg_ptr )
{
    char buf[BUFSIZ];
    int buflen;

    buflen = ::vsprintfT( buf,fmt,arg_ptr );
    return write( buf,buflen );
}   // TFile::vprintf



int TFile::read( void *buff, int bufflen )
{
    int r;

    invalidateBuffer();
    r = iread( buff,bufflen );
    if (r > 0)
	filePos += r;

    assert( filePos <= fileLen );
    return r;
}   // TFile::read



int TFile::iread( void *buff, int bufflen )
//
//  must not change filePos!
//
{
    assert( mode == mread  ||  mode == mreadwrite );
    return ::readT( handle,buff,bufflen );
}   // TFile::iread



int TFile::fillBuff( void )
//
//  must not change filePos!
//
{
    int blk;
    int toRead;

#ifdef TRACE_ALL
    printfT( "TFile::fillBuff()\n" );
#endif

    if (buffEof)
	return buffNdx < buffEnd;

    if (buffEnd-buffNdx >= buffSize/2)
	return 1;

#ifdef TRACE_ALL
    printfT( "TFile::fillBuff() -- 2\n" );
////    printfT( "-- '%s','%s',%d\n", buffer,buffer+buffNdx,buffEnd-buffNdx );
#endif

    memmove( buffer, buffer+buffNdx, buffEnd-buffNdx );
    buffEnd = buffEnd - buffNdx;
    buffNdx = 0;
#ifdef TRACE_ALL
////    printfT( "++ '%s'\n", buffer );
#endif
    toRead = (buffSize-buffEnd); //// & 0xfc00;
    blk = iread( buffer+buffEnd, toRead );
    if (blk > 0)
	buffEnd += blk;         // getcc() increments filePos 
    else
	buffEof = 1;            // -> nothing left to read!
    buffer[buffEnd] = '\0';

#ifdef DEBUG_ALL
    printfT( "TFile::fillBuff() = %d,%d,%d\n",blk,buffNdx,buffEnd );
#endif
    return buffEnd > 0;
}   // TFile::fillBuff



int TFile::getcc( void )
{
#ifdef TRACE_ALL
    //// printfT( "TFile::getcc()\n" );
#endif
    if (buffNdx >= buffEnd) {
	if ( !fillBuff())
	    return -1;
    }
    ++filePos;
    return buffer[buffNdx++];
}   // TFile::getcc



char *TFile::fgets( char *buff, int bufflen, int skipCrLf )
//
//  '\0' will always be skipped!
//  If SkipCrLf, then also \r & \n are skipped
//
{
    char *p;
    int  n;
    int  r;

#ifdef TRACE_ALL
    printfT( "TFile::fgets(.,%d) %s\n",bufflen,fname );
#endif

    p = buff;
    n = 0;
    for (;;) {
	if (n >= bufflen-2) {     // if Buffer exhausted return (there is no \n at eob)
	    if (skipCrLf) {
		do
		    r = getcc();
		while (r > 0  &&  r != '\n');
	    }
	    break;
	}

	r = getcc();
	if (r < 0) {              // EOF!
	    if (n == 0) {
		*p = '\0';
		return( NULL );
	    }
	    else
		break;
	}
	else if (r != 0) {
	    if ( ! (skipCrLf  &&  (r == '\r'  ||  r == '\n'))) {
		*(p++) = (unsigned char)r;
		++n;
	    }
	    if (r == '\n')
		break;
	}
    }
    *p = '\0';
    return buff;
}   // TFile::fgets



int TFile::scanf( const char *fmt, void *a1 )
//
//  Scan one argument via sscanf.  Extension of this function to any number
//  of arguments does not work, because "%n" must be added (and an argument...)
//  Perhaps one argument is also ok, because what happened if one wants to
//  scan two arguments, but the second failed (what will "%n" display?)
//  -  sollen besondere Zeichen gescannt werden, so sollte das immer mit "%[...]"
//     gemacht werden, da ein einzelnes Zeichen, das nicht paát die Zuweisung "%n"
//     verhindert!
//  trick:  the internal read buffer is always kept at least half full,
//          therefor scanf can work just like sscanf!
//
{
    int  fields;
    int  cnt;
    char fmt2[200];

#ifdef TRACE_ALL
    printfT( "TFile::scanf1(%s,...) %d,%d\n",fmt,buffNdx,buffEnd );
#endif

    if ( !fillBuff())
	return EOF;

    strcpy( fmt2,fmt );
    strcat( fmt2,"%n" );

    cnt = 0;
    fields = ::sscanfT( (char *)buffer+buffNdx, fmt2, a1,&cnt );
#ifdef DEBUG_ALL
    printfT( "TFile::scanf1()='%.20s' %d,%d\n",buffer+buffNdx,cnt,fields );
#endif
    if (cnt == 0)
	fields = 0;
    else {
	buffNdx += cnt;
	filePos += cnt;
	assert( filePos <= fileLen );
    }
    return fields;
}   // TFile::scanf
