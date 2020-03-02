//  $Id: pop3.hh 1.4 1999/08/29 12:57:06 Hardy Exp Hardy $
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


#ifndef __POP3_HH__
#define __POP3_HH__


int getMail( const char *host, const char *userid, const char *password, int port );
int getMailStatus( const char *host, const char *userid, const char *password, int port );


#endif   // __POP3_HH__
