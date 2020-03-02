//  $Id: rgfile.hh 1.1 1998/04/12 16:24:54 hardy Exp hardy $
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


#ifndef __RGFILE_HH__
#define __RGFILE_HH__


#include <stdio.h>


class TFile {

public:
    enum TMode {mnotopen,mread,mwrite,mreadwrite};
    enum OMode {otext,obinary};

private:
    int handle;
    int buffEnd;
    int buffNdx;
    int buffEof;

protected:
    TMode mode;
    unsigned char *buffer;
    int buffSize;
    const char *fname;
    long filePos;
    long fileLen;
    
public:
    TFile( void );
    virtual ~TFile();

    virtual void close( int killif0=0, int forceKill=0 );
    void remove( void );
    int  open( const char *name, TMode mode, OMode textmode, int create=0 );
    int  write( const void *buff, int bufflen );
    int  putcc( char c );
    int  fputs( const char *s );
    int  printf( const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
    int  vprintf( const char *fmt, va_list arg_ptr );
    int  read( void *buff, int bufflen );
    int  getcc( void );
    char *fgets( char *buff, int bufflen, int skipCrLf=0 );
    int  scanf( const char *fmt, void *a1 );
////    int  scanf( const char *fmt, ... ) __attribute__ ((format (scanf, 2, 3)));
    int  flush( void ) { return 1; };
    virtual int  truncate( long length );
    virtual long seek( long offset, int origin );
    virtual long tell( void );

    const char *getName( void ) { return fname; };
    int  isOpen( void ) { return mode != mnotopen; };

private:
    int  fillBuff( void );

protected:
    void invalidateBuffer( void ) { buffEof = buffNdx = buffEnd = 0; };
    virtual int  iread( void *buff, int bufflen );
    virtual int  iwrite( const void *buff, int bufflen );
    virtual long iseek( long offset, int origin );
};



class TFileTmp: public TFile {

private:
    int memBuffIncrement;
    int memBuffSize;
    unsigned char *memBuff;

public:
    TFileTmp( void );
    ~TFileTmp();

    virtual void close( void );
    int  open( int increment=4096 );
    virtual int  truncate( long offset );

private:
    virtual int  iread( void *buff, int bufflen );
    virtual int  iwrite( const void *buff, int bufflen );
    virtual long iseek( long offset, int origin );
};


#endif   // __RGFILE_HH__
