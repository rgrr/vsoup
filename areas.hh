//  $Id: areas.hh 1.13 1999/08/29 12:50:53 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __AREAS_HH__
#define __AREAS_HH__


#include <rgfile.hh>
#include <rgsema.hh>


#define AREAS_FIFOSIZE  15


class TAreas {

private:
    TAreas( const TAreas &right );      // copy constructor not allowed !
    operator = (const TAreas &right);   // assignment operator not allowed !
    TAreas( void );                     // not allowed !
public:
    TAreas( const char *areasName, const char *msgNamePattern );
    ~TAreas();

    int  msgWrite( const char *buf, int buflen );
    int  msgPrintf( const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
    void closeAll( void );
    void msgStart( const char *id, const char *format );
    void msgStop( void );

protected:
    const char *msgNamePattern;
    TFile areasF;

private:
    TFile msgF;
    int  msgCounter;
    int  msgStarted;
    long msgLenPos;

    struct {
	const char *id;
	const char *format;
	const char *filename;
    } fifo[AREAS_FIFOSIZE];
    int fifoNext;
    int fifoMsgF;
};
    

class TAreasMail: public TAreas  {
private:
    TAreasMail( void );                         // not allowed !
    TAreasMail( const TAreasMail &right );      // copy constructor not allowed !
    operator = (const TAreasMail &right);       // assignment operator not allowed !
public:
    TAreasMail( const char *areasName, const char *msgNamePattern ); ///: TAreas(areasName,msgNamePattern) {}
    ~TAreasMail();
    void mailOpen( const char *title );
    void mailStart();
    void mailStop( void );
    int  mailPrintf( const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
    int  mailPrintf1( int echoWin, const char *fmt, ... ) __attribute__ ((format (printf, 3, 4)));
    void mailException( void ) { mailExcept = 1; }
    void closeAll( void );
    void forceMail( void ) { mailForced = 1; }
private:
    int mailStarted;
    int mailFirstLine;
    int mailForced;
    int mailExcept;
    TFile mailF;
    const char *mailName;
    TSemaphor mpSema;
};


#endif   // __AREAS_HH__
