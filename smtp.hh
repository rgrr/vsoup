//  $Id: smtp.hh 1.4 1999/08/29 12:58:00 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __SMTP_HH__
#define __SMTP_HH__


#include <rgsocket.hh>


int  smtpConnect( TSocket &socket );
void smtpClose( TSocket &socket );
int  smtpMail( TSocket &socket, TFile &file, size_t bytes );


#endif   // __SMTP_HH__
