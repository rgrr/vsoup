//  $Id: rgmts.cc 1.23 1998/04/12 16:24:31 hardy Exp $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@ibm.net.
//
//  This file is part of soup++ for OS/2.  Soup++ including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#include "rgmtreg.hh"
#include "rgsema.hh"



static TSemaphor regSema;



regexp *regcompT( const char *exp )
{
    regexp *res;

    regSema.Request();
    res = regcomp( exp );
    regSema.Release();
    return res;
}   // regcompT



int regexecT( const regexp *cexp, const char *target )
{
    int res;

    regSema.Request();
    res = regexec( cexp,target );
    regSema.Release();
    return res;
}   // regexecT
