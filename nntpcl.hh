//  $Id: nntpcl.hh 1.18 1999/08/29 12:56:13 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __NNTPCL_HH__
#define __NNTPCL_HH__


#include <assert.h>

#include <rgmts.hh>
#include <rgsocket.hh>


//--------------------------------------------------------------------------------
//
//  Diverse NNTP-Fkts zur Klasse zusammengefa·t
//
//  Problem dieser Klasse ist:
//  - die ZustÑnde kînnen/mÅssen von extern gesetzt werden
//
class TNntp:  private TSocket {
public:
    enum Res   { nok,ok,killed,notavail,nok_goon };    // nok must be 0, ok 1
    int artNotAvail;

public:
    TNntp( void );
    ~TNntp();
    TNntp( const TNntp &right );          // copy constructor not allowed !
    operator = (const TNntp &right);      // assignment operator not allowed !

    void  setHelper( void (*xref)(const char *xrefLine),
		     long (*killQ)(const char *groupName, const char *headerLine) );
    Res  open( const char *nntpServer, const char *nntpUser,
	       const char *nntpPasswd, int nntpPort, int openXmt );
    void close( int sendQuit=1 );
    int  isReadOnly( void ) { return readOnly; }

    Res  getNewGroups( const char *nntpTimeFile, int changeFile );
    Res  getOverview( long first, long last );
    Res  setActGroup( const char *grpname, long &Cnt, long &lo, long &hi);
    Res  nextArticle( long *next );
    Res  postArticle( TFile &file, size_t bytes );
    Res  ihaveArticle( TFile &file, size_t bytes, const char *msgId );
    Res  getArticle( void );
    Res  getXhdr( const char *headerField, long first, long last,
		  int (*callback)(int operation, const char *line) );

    TFileTmp &getTmpF( void ) { return tmpF; }

    void selectArticle( const char *grpname, long artNum=-1,
			int doKill=0, long artFirst=-1, long artHi=-1 );
    long article( void ) { return selArticle; }
    long artFirst( void ) { return nntpArtFirst; };
    long artHi( void ) { return nntpArtHi; };
    long nntpArticle( void ) { return selNntpArticle; }
    const char *groupName( void ) { return selGroup; }
    const char *getLastErrMsg( void );

private:
    int   readOnly;
    TFileTmp tmpF;
    const char *actGroup;                                 // group activated at nntp server
    const char *selGroup;
    long selArticle;
    long selNntpArticle;
    long nntpArtFirst;
    long nntpArtHi;
    int  killEnabled;
    char lastErrMsg[BUFSIZ+100];                          // buffer for last error msg
    const char *user;
    const char *passwd;

    void (*xrefHook)( const char *xrefLine );             // hook for xref processing
    long (*killQHook)( const char *groupName,             // hook for killfile processing
		       const char *headerLine );  

    Res request( const char *request, char *reply, size_t replySize, int expReply );

    Res _getHead( void );
    Res _getBody( void );
};

    
#endif   // __NNTPCL_HH__
