//  $Id: rgsema.hh 1.11 1997/12/28 19:20:53 hardy Exp hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@ibm.net.
//
//  This file is part of soup++ for OS/2.  Soup++ including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __SEMA_HH__
#define __SEMA_HH__


#if defined(OS2)  &&  defined(__MT__)

#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#include <os2.h>

class TSemaphor {
private:
    HMTX Sema;
public:
    TSemaphor( void )    { DosCreateMutexSem( (PSZ)NULL, &Sema, 0, FALSE ); }
    ~TSemaphor()         { DosCloseMutexSem( Sema ); }
    int  Request( long timeMs = SEM_INDEFINITE_WAIT ) { return (DosRequestMutexSem(Sema,timeMs) == NO_ERROR); }
    void Release( void ) { DosReleaseMutexSem( Sema ); }
};


class TEvSemaphor {
private:
    HEV Sema;
public:
    TEvSemaphor( void )                            { DosCreateEventSem( (PSZ)NULL, &Sema, 0, FALSE ); }
    ~TEvSemaphor()                                 { DosCloseEventSem( Sema ); }
    void Post( void )                              { DosPostEventSem( Sema ); }
    void Wait( long timeMs = SEM_INDEFINITE_WAIT,
	       int resetSema = 1 )                 { if (DosWaitEventSem(Sema,timeMs) == 0) { if (resetSema) Reset();} }
    void Reset( void )                             { unsigned long ul;  DosResetEventSem( Sema,&ul ); }
};

#define blockThread()    DosEnterCritSec()
#define unblockThread()  DosExitCritSec()


/////  hier anpassen
/////  und hoffentlich ist der Rest syntaktisch korrekt

#elif defined(__WIN32__)  &&  defined(__MT__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winproc.h>

class TSemaphor {
private:
    HANDLE Sema;
public:
    TSemaphor( void )    { Sema = CreateMutex( 0,FALSE,0 ); }
    ~TSemaphor()         { CloseHandle( Sema ); }
    int  Request( long timeMs = INFINITE ) { return (WaitForSingleObject(Sema,timeMs) == 0); }
    void Release( void ) { ReleaseMutex( Sema ); }
};


class TEvSemaphor {
private:
    HANDLE Sema;
public:
    TEvSemaphor( void )                            { Sema = CreateEvent( 0,FALSE,FALSE,0 ); }
    ~TEvSemaphor()                                 { CloseHandle( Sema ); }
    void Post( void )                              { SetEvent( Sema ); }
    void Wait( long timeMs = INFINITE,
	       int resetSema = 1 )                 { if (WaitForSingleObject(Sema,timeMs) == 0) { if (resetSema) Reset();} }
    void Reset( void )                             { ResetEvent( Sema ); }
};

#ifdef DEFGLOBALS
TSemaphor wincrit;
#else
extern TSemaphor wincrit;
#endif

#define blockThread()    wincrit.Request()
#define unblockThread()  wincrit.Release()

#else

class TSemaphor {
public:
    TSemaphor( void )    {}
    ~TSemaphor()         {}
    int  Request( long timeMs=0 ) { timeMs = 0; return 1; }
    void Release( void ) {}
};


class TEvSemaphor {
public:
    TEvSemaphor( void )      {}
    ~TEvSemaphor()           {}
    void Post( void )        {}
    void Wait( long timeMs=0 ) { timeMs = 0; }
    void Reset( void )       {}
};

#define blockThread()
#define unblockThread()

#endif


#endif  // __SEMA_HH__
