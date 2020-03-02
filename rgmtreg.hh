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


#ifndef __RGMTREG_HH__
#define __RGMTREG_HH__

#include <regexp.h>

regexp *regcompT( const char *exp );
int regexecT( const regexp *cexp, const char *target );

#endif   // __RGMTREG_HH__
