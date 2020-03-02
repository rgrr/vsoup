//  $Id: rgmts.hh 1.20 1998/04/12 16:24:45 hardy Exp hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@ibm.net.
//
//  This file is part of soup++ for OS/2.  Soup++ including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  multithreading save library functions
//
//  requires stdarg.h and stio.h
//


#ifndef __RGMTS_HH__
#define __RGMTS_HH__

#if defined(__MT__)
#include <errno.h>
#define BEGINTHREAD(f,arg)  { while (_beginthread((f),NULL,200000,(arg)) == -1) { \
                                 if (errno != EINVAL) { \
				     perror("beginthread"); \
				     exit( EXIT_FAILURE ); }}}
#else
#define BEGINTHREAD(f,arg)  (f)(arg)
#endif

#include "rgsema.hh"        // all semaphore classes included !


int hprintfT( int handle, const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
int hputsT( const char *s, int handle );
int sprintfT( char *dst, const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
int vsprintfT( char *dst, const char *fmt, va_list arg_ptr );
int printfT( const char *fmt, ... ) __attribute__ ((format (printf, 1, 2)));
int vprintfT( const char *fmt, va_list arg_ptr );
int vsnprintfT( char *dst, size_t n, const char *fmt, va_list arg_ptr );
int sscanfT( const char *src, const char *fmt, ... ) __attribute__ ((format (scanf, 2, 3)));

int openT( const char *name, int oflag );
int closeT( int handle );
int readT( int handle, void *buffer, int len );
int writeT( int handle, const void *buffer, int len );
long lseekT( int handle, long offset, int origin );
int ftruncateT( int handle, long length );

size_t fwriteT( const void *buffer, size_t size, size_t count, FILE *out );
FILE *popenT( const char *command, const char *mode );
int pcloseT( FILE *io );
int removeT( const char *fname );
int renameT( const char *oldname, const char *newname );

const char *xstrdup( const char *src );
void xstrdup( const char **dst, const char *src );


//
//  multithreading save counter
//
class TProtCounter {
private:
    TProtCounter( const TProtCounter &right );   // not allowed !
    operator = ( const TProtCounter &right );    // not allowed !
public:
    TProtCounter( void ) { Cnt = 0; }
    ~TProtCounter() {}
    operator = ( const unsigned long &right ) { Cnt = right;  return *this; }

    TProtCounter & operator += (const unsigned long &offs) { blockThread();  Cnt += offs;  unblockThread();  return *this;  }
    TProtCounter & operator ++(void) { blockThread();  ++Cnt;  unblockThread();  return *this; }
    TProtCounter & operator --(void) { blockThread();  --Cnt;  unblockThread();  return *this; }
    operator unsigned long() { unsigned long res;  blockThread();  res = Cnt;  unblockThread();  return res; }
private:
    unsigned long Cnt;
};


#endif   // __RGMTS_HH__
