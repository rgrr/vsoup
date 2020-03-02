//  $Id: kill.hh 1.12 1999/08/29 12:54:22 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __KILL_HH__
#define __KILL_HH__


#include <regexp.h>

#include <rgfile.hh>


class MatchObj {    // abstract class?
public:
    MatchObj( void ) {}
    virtual ~MatchObj() {}
    virtual int doesMatch( const char *line ) = 0;
};


class TKillFile {
private:
    //
    // kill regular expression
    //
    typedef struct aExp {
	struct aExp *next;	// next in list
	MatchObj *macho;        // matching object
	long points;
    } Exp;

    //
    // kill file entry for a newsgroup
    //
    typedef struct aGroup {
	struct aGroup *next;	// next in list
	regexp *grpPat; 	// newsgroup pattern
	Exp *expList;		// list of kill expressions
    } Group;

    Group *groupKillList;	// list of group specific kills
    TSemaphor sema;
    Group *actGroupList;
    const char *actGroupName;
    long killThreshold;

    void killGroup( Group *gp );
    Exp *genRegExp(const char *searchIn, const char *searchFor);
    Group *buildActGroupList( const char *groupName );
    void stripBlanks( char *line );
    int readLine( char *buf, int n, TFile &inf, int &lineNum );

public:
    TKillFile( void );
    ~TKillFile();
    TKillFile( const TKillFile &right );    // copy constructor not allowed !
    operator = (const TKillFile &right);    // assignment operator not allowed !

    int readFile( const char *killFile );
    long matchLine( const char *groupName, const char *line );
    int doKillQ( const char *groupName );
};
    

#endif   // __KILL_HH__
