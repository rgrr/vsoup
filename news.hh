//  $Id: news.hh 1.6 1999/08/29 12:54:34 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __NEWS_HH__
#define __NEWS_HH__


int sumNews( void );
int getNews( int strategy );
int catchupNews( long numKeep );
long getNewsStatus( void );


//
//  file names
//
#define FN_COMMAND "commands"


#endif   // __NEWS_HH__
