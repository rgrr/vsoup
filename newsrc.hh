//  $Id: newsrc.hh 1.11 1999/08/29 12:55:15 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//


#ifndef __NEWSRC_HH__
#define __NEWSRC_HH__


#include <rgmts.hh>


#define NEWSRC_HASHSIZE   4095


class TNewsrc {
private:
    //
    //  article number range in the .newsrc file
    //
    typedef struct aRange {
	struct aRange *next;	// pointer to next
	long lo, hi;		// article number range */
    } Range;

    //
    //  newsgroup entry in the .newsrc file
    //
    typedef struct aGroup {
	struct aGroup *next;		// pointer to next
	struct aGroup *hashNext;        // pointer to next in hash list
	const char *name;		// newsgroup name
	Range *readList;		// list of read article ranges
        char subscribed;		// subscribed flag
        char initialCatchup;            // do an initial catchup!
    } Group, *pGroup;

    int fileChanged;                    // (internal) file has been changed (-> rewrite file)
    pGroup groups;                      // list of .newsrc entries.
    const char *filename;               // name of newsrc-file
    TSemaphor sema;
    const char *cacheGroupName;         // Cache for active group
    pGroup cacheGroup;                  //          "
    int fileRead;
    pGroup addGroupP;

    pGroup hashTab[NEWSRC_HASHSIZE];
    
public:
    TNewsrc( void );
    ~TNewsrc();
    TNewsrc( const TNewsrc &right );    // copy constructor not allowed !
    operator = (const TNewsrc &right);  // assignment operator not allowed !

    int   readFile( const char *newsrcFile );
    int   writeFile( void );
    const char *grpFirst( void );
    const char *grpNext( const char *prevGroupName );
    int   grpSubscribed( const char *groupName );
    int   grpExists( const char *groupName );
    void  grpUnsubscribe( const char *groupName );
    void  grpFixReadList( const char *groupName, long groupLo, long groupHi,
                          long initialCatchupCnt=-1 );
    long  grpFirstUnread( const char *groupName, long groupLo );
    int   artIsRead( const char *groupName, long artNum );
    void  artMarkRead( const char *groupName, long artNum );
    void  grpCatchup( const char *groupName, long groupLo, long groupHi, long numKeep );
    void  *grpAdd( const char *groupName, int subscribe=0 );

private:
    Range *getReadList( TFile &nrcFile );
    void putReadList( TFile *fd, Range *head );
    pGroup getGroupP( const char *groupName );
};


#endif   // __NEWSRC_HH__
