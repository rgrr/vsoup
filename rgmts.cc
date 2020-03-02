//  $Id: rgmts.cc 1.23 1998/04/12 16:24:31 hardy Exp $
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

#include "rgmts.hh"
#include "rgsema.hh"



//--------------------------------------------------------------------------------



TSemaphor sysSema;   // damit werden einige Systemaufrufe serialisiert...



size_t fwriteT( const void *buffer, size_t size, size_t count, FILE *out )
{
    size_t res;

    sysSema.Request();
    res = fwrite( buffer,size,count,out );
    sysSema.Release();
    return res;
}   // fwriteT



FILE *popenT( const char *command, const char *mode )
{
    FILE *res;

#ifdef TRACE_ALL
    printfT( "popenT(%s,%s)\n", command,mode );
#endif

    sysSema.Request();
    res = popen( command,mode );
    sysSema.Release();
    return res;
}   // popenT



int pcloseT( FILE *io )
{
    int res;

#ifdef TRACE_ALL
    printfT( "pcloseT(%p)\n", io );
#endif

    sysSema.Request();
    res = pclose( io );
    sysSema.Release();
    return res;
}   // pcloseT



int removeT( const char *fname )
{
    int res;

#ifdef TRACE_ALL
    printfT( "removeT(%s)\n", fname );
#endif

    sysSema.Request();
    res = remove( fname );
    sysSema.Release();
    return res;
}   // removeT



int renameT( const char *oldname, const char *newname )
{
    int res;

#ifdef TRACE_ALL
    printfT( "renameT(%s,%s)\n",oldname,newname );
#endif

    sysSema.Request();
    res = rename( oldname,newname );
    sysSema.Release();
    return res;
}   // renameT



int openT( const char *name, int oflag )
//
//  open file with mode oflag.
//  - shared write access is denied
//  - if file will be created, it will permit read/write access
//
{
    int res;

#ifdef TRACE_ALL
    printfT( "openT(%s,%d)\n", name,oflag );
#endif

    sysSema.Request();
    res = sopen( name, oflag, SH_DENYWR, S_IREAD | S_IWRITE );
    sysSema.Release();
    return res;
}   // openT



int closeT( int handle )
{
    int res;

#ifdef TRACE_ALL
    printfT( "closeT(%d)\n", handle );
#endif

    sysSema.Request();
    res = close( handle );
    sysSema.Release();
    return res;
}   // closeT



int readT( int handle, void *buffer, int len )
{
    int res;

#ifdef TRACE_ALL
    printfT( "readT(%d,%p,%d)\n",handle,buffer,len );
#endif

    sysSema.Request();
    res = read( handle,buffer,len );
    sysSema.Release();
    return res;
}   // readT



int writeT( int handle, const void *buffer, int len )
{
    int res;

#ifdef TRACE_ALL
    printfT( "writeT(%d,%p,%d)\n",handle,buffer,len );
#endif

    sysSema.Request();
    res = write( handle,buffer,len );
    sysSema.Release();
    return res;
}   // writeT



long lseekT( int handle, long offset, int origin )
{
    long res;

#ifdef TRACE_ALL
    printfT( "lseekT(%d,%ld,%d)\n",handle,offset,origin );
#endif

    sysSema.Request();
    res = lseek( handle,offset,origin );
    sysSema.Release();
    return res;
}   // lseekT



int ftruncateT( int handle, long length )
{
    int res;

#ifdef TRACE_ALL
    printfT( "ftruncateT(%d,%ld)\n",handle,length );
#endif

    sysSema.Request();
    res = ftruncate( handle,length );
    sysSema.Release();
    return res;
}   // ftruncateT



//--------------------------------------------------------------------------------



int hprintfT( int handle, const char *fmt, ... )
{
    va_list ap;
    char buf[BUFSIZ];
    int  buflen;

    sysSema.Request();
    va_start( ap, fmt );
    buflen = vsprintf( buf, fmt, ap );
    va_end( ap );
    if (buflen > 0)
	buflen = write( handle,buf,buflen );
    sysSema.Release();
    return buflen;
}   // hprintfT



int hputsT( const char *s, int handle )
{
    int len, res;

    sysSema.Request();
    len = strlen(s);
    res = write( handle,s,len );
    sysSema.Release();
    return (res == len) ? len : EOF;
}   // hputsT



int sprintfT( char *dst, const char *fmt, ... )
{
    va_list ap;
    int  buflen;

    sysSema.Request();
    *dst = '\0';
    va_start( ap, fmt );
    buflen = vsprintf( dst, fmt, ap );
    va_end( ap );
    sysSema.Release();
    
    return buflen;
}   // sprintfT



int printfT( const char *fmt, ... )
{
    va_list ap;
    char buf[BUFSIZ];
    int  buflen;

    sysSema.Request();
    va_start( ap, fmt );
    buflen = vsprintf( buf, fmt, ap );
    va_end( ap );
    if (buflen > 0)
	write( STDOUT_FILENO, buf,buflen );
    sysSema.Release();
    return buflen;
}   // printfT



int vprintfT( const char *fmt, va_list arg_ptr )
{
    char buf[BUFSIZ];
    int buflen;

    sysSema.Request();
    buflen = vsprintf( buf,fmt,arg_ptr );
    if (buflen > 0)
	write( STDOUT_FILENO, buf,buflen );
    sysSema.Release();
    return buflen;
}   // vprintfT



int vsprintfT( char *dst, const char *fmt, va_list arg_ptr )
{
    int buflen;

    sysSema.Request();
    buflen = vsprintf( dst,fmt,arg_ptr );
    sysSema.Release();
    return buflen;
}   // vsprintfT



int vsnprintfT( char *dst, size_t n, const char *fmt, va_list arg_ptr )
{
    int buflen = 0;

    if (dst) {
        *dst = '\0';
        sysSema.Request();
        buflen = vsnprintf( dst, n, fmt, arg_ptr );
        sysSema.Release();
    }
    return buflen;
}   // vsnprintfT



int sscanfT( const char *src, const char *fmt, ... )
{
    va_list ap;
    int fields;

    sysSema.Request();
    va_start( ap, fmt );
    fields = vsscanf( src, fmt, ap );
    va_end( ap );
    sysSema.Release();
    return fields;
}   // sscanfT



//--------------------------------------------------------------------------------



const char *xstrdup( const char *src )
{
    int len;
    char *dst;

    if (src == NULL)
	return NULL;
    
    len = strlen(src)+1;
    dst = new char [len];
    if (dst != NULL)
	memcpy( dst,src,len );
    else {
	hprintfT( STDERR_FILENO,"\nxstrdup failed\nprogram aborted\n" );
	exit( 3 );
    }
    return dst;
}   // xstrdup



void xstrdup( const char **dst, const char *src )
{
    if (*dst != NULL)
	delete *dst;

    if (src != NULL)
	*dst = xstrdup( src );
    else
	*dst = NULL;
}   // xstrdup
