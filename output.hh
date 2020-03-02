//  $Id: output.hh 1.1 1999/08/29 12:56:49 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __OUTPUT_HH__
#define __OUTPUT_HH__

#include <stdlib.h>

void OutputInit( void );
void OutputOpenWindows( int winMask, const char *newsTitle, const char *mailTitle, const char *xmitTitle );
void Output( int wn, const char *fmt, ... ) __attribute__ ((format(printf,2,3)));
void OutputV( int wn, const char *fmt, va_list arg_ptr );
void OutputClrEol( int wn );
void OutputCR( int wn );
void OutputExit( void );


//
//  windows
//
#define OW_NEWS 3
#define OW_MAIL 1
#define OW_XMIT 2
#define OW_MISC 0
#define OW_MAX  3     // last window number


#endif   // __OUTPUT_HH__
