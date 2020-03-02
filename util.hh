//  $Id: util.hh 1.14 1999/08/29 12:58:41 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  NNTP client routines
//


#ifndef __UTIL_HH__
#define __UTIL_HH__


#include <rgfile.hh>


const char *getHeader( TFile &handle, const char *header );
unsigned hashi( const char *src, unsigned tabSize );
int nhandles( int depth=0 );
const char *extractAddress( const char *src );
char *findAddressSep( const char *src );
int isHeader( const char *buf, const char *header );
void urlDecode( char *s );


#endif   // __UTIL_HH__
