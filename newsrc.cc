//  $Id: newsrc.cc 1.27 1999/08/29 13:08:30 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  rg110796:
//  - introduction of a hash list because performance was real bad
//  - grpFirst/grpNext now returning only subscribed groups
//  - newsrc is written if there were any changes (onyl then!)
//


#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rgfile.hh>
#include <rgmts.hh>

#include "newsrc.hh"
#include "util.hh"



TNewsrc::TNewsrc( void )
{
    int i;
    
#ifdef TRACE_ALL
    printfT( "TNewsrc::TNewsrc()\n" );
#endif
    groups         = NULL;
    filename       = xstrdup("");
    cacheGroup     = NULL;
    cacheGroupName = NULL;
    addGroupP      = NULL;
    fileRead       = 0;
    for (i = 0;  i < NEWSRC_HASHSIZE;  ++i)
	hashTab[i] = (pGroup)NULL;
}   // TNewsrc::TNewsrc



TNewsrc::~TNewsrc()
{
    Range *rp1, *rp2;
    pGroup gp1, gp2;
    
    delete filename;
    gp1 = groups;
    while (gp1 != NULL) {
	rp1 = gp1->readList;
	while (rp1 != NULL) {
	    rp2 = rp1->next;
	    delete rp1;
	    rp1 = rp2;
	}
	gp2 = gp1->next;
	delete gp1->name;
	delete gp1;
	gp1 = gp2;
    }
}   // TNewsrc::~TNewsrc



TNewsrc::Range *TNewsrc::getReadList( TFile &nrcFile )
//
//  Read the article numbers from a .newsrc line.
//
{
    Range *pLast, *rp, *head;
    int  c;
    long lo,hi;
    char buf[10];

    /* Initialize subscription list */
    pLast = NULL;
    head = NULL;

    /* Expect [ \n] */
    c = nrcFile.getcc();

    while (c != '\n'  &&  c != EOF) {
	/* Expect number */
	if (nrcFile.scanf("%ld",&lo) != 1) {
	    nrcFile.fgets( buf,sizeof(buf),1 );
	    break;
	}
	lo = (lo >= 0) ? lo : 0;

	/* Get space for new list entry */
	rp = new Range;
	rp->next = NULL;

	/* Expect [ ,-,\n] */
	c = nrcFile.getcc();
	if (c == '-') {
	    /* Is a range */
	    /* Expect number */
	    if (nrcFile.scanf("%ld",&hi) != 1) {
		nrcFile.fgets( buf,sizeof(buf),1 );
		break;
	    }
	    hi = (hi >= 0) ? hi : 0;

	    rp->lo = lo;
	    rp->hi = hi;

	    /* Reverse them in case they're backwards */
	    if (hi < lo) {
		rp->lo = hi;
		rp->hi = lo;
	    }
	    if (rp->lo == 0)  //???
		rp->lo = 1;

	    /* Expect [,\n] */
	    c = nrcFile.getcc();
	} else {
	    /* Not a range */
	    rp->lo = rp->hi = lo;
	}
	if (lo <= 0) {   //???
	    delete rp;
	    continue;
	}

	/* Check if range overlaps last one */
	if (pLast != NULL  &&  rp->lo <= pLast->hi + 1) {
	    /* Combine ranges */
	    if (rp->lo < pLast->lo)
		pLast->lo = rp->lo;
	    if (rp->hi > pLast->hi)
		pLast->hi = rp->hi;

	    /* Free old (ehm new?) one */
	    delete rp;
	} else {
	    /* No overlap, update pointers */
	    if (pLast == NULL) {
		head = rp;
	    } else {
		pLast->next = rp;
	    }
	    pLast = rp;
	}
    }

    return head;
}   // TNewsrc::getReadList



int TNewsrc::readFile( const char *newsrcFile )
//
//  Read the .newsrc file.
//  Return 0 on error (otherwise 1)
//
{
    TFile nrcFile;
    char groupName[BUFSIZ];
    char buf[10];

#ifdef TRACE_ALL
    printfT("TNewsrc::readFile(%s)\n",newsrcFile);
#endif
    assert( !fileRead );

    filename = xstrdup(newsrcFile);

    /* Open it */
    if ( !nrcFile.open(newsrcFile,TFile::mread,TFile::otext)) {
	hprintfT( STDERR_FILENO, "cannot open %s\n", newsrcFile );
	fileRead = 1;
	return 0;
    }

    sema.Request();

    /* Read newsgroup entry */
    for (;;) {
	pGroup np;
	int res;

        res = nrcFile.scanf("%[^:! \t\n]", groupName);
        
	if (res == 0) {
	    nrcFile.fgets( buf,sizeof(buf),1 );
	    continue;
	}
	if (res != 1)
	    break;

#ifdef DEBUG_ALL
	printfT( "TNewsrc::readfile(): %s\n",groupName );
#endif
	if (strchr(groupName,',') != NULL) {
	    hprintfT( STDERR_FILENO,"ill groupname in %s: %s\n", newsrcFile,groupName );
	    nrcFile.fgets( buf,sizeof(buf),1 );
	    continue;
	}
	else if (grpExists(groupName)) {
	    hprintfT( STDERR_FILENO,"groupname %s found twice in\n        %s - second occurence removed\n",
		      groupName,newsrcFile );
	    nrcFile.fgets( buf,sizeof(buf),1 );
	    continue;
	}

	if (groupName[0] == '\0')
	    break;

	//
	//  Allocate a new entry
	//
	np = (pGroup)grpAdd( groupName );
	*buf = '\0';
        nrcFile.scanf( "%1[:!]",buf );
	if (*buf == '\0') {
	    /* The user didn't end the line with a colon. */
	    nrcFile.fgets( buf,sizeof(buf),1 );
            np->subscribed = 1;
            np->initialCatchup = 1;
        } else {
	    /* Parse subscription list */
            np->subscribed = (*buf == ':');
            np->readList = getReadList(nrcFile);
            np->initialCatchup = (np->readList == NULL);
        }

#ifdef DEBUG_ALL
	putReadList( NULL, np->readList );
#endif
    }

    nrcFile.close();
    fileRead = 1;
    fileChanged = 0;

    sema.Release();

    return 1;
}   // TNewsrc::readFile



void TNewsrc::putReadList( TFile *fd, Range *head )
//
//  Write the article numbers for a .newsrc entry.
//  if fd==NULL, send output to console
//
{
    if (head == NULL) {
	if (fd != NULL)
	    fd->putcc('0');
	else
            printfT("0");
    }
    else {
	while (head != NULL) {
	    if (head->lo == head->hi) {
		if (fd != NULL)
		    fd->printf("%ld", head->lo);
		else
		    printfT("%ld", head->lo);
	    }
	    else {
		if (fd != NULL)
		    fd->printf("%ld-%ld", head->lo, head->hi);
		else
		    printfT("%ld-%ld", head->lo, head->hi);
	    }
	    head = head->next;
	    if (head != NULL) {
		if (fd != NULL)
		    fd->putcc(',');
		else
		    printfT(",");
	    }
	}
    }
    if (fd != NULL)
	fd->putcc('\n');
    else
	printfT("\n");
}   // TNewsrc::putReadList



int TNewsrc::writeFile(void)
//
//  Rewrite the updated .newsrc file
//
{
    char oldFile[FILENAME_MAX];
    TFile nrcFile;
    pGroup np;

#ifdef TRACE_ALL
    printfT( "TNewsrc::writeFile()\n" );
#endif
    if (filename[0] == '\0')
	return 1;                        // successful (cause nothing to do)
    if (groups == NULL  ||  !fileRead  ||  !fileChanged)
	return 1;

    sema.Request();

    //
    //  Back up old .newsrc file.
    //
    sprintfT(oldFile, "%s.old", filename);
    removeT(oldFile);
    renameT(filename, oldFile);

    if ( !nrcFile.open(filename,TFile::mwrite,TFile::otext,1)) {
	hprintfT( STDERR_FILENO, "cannot write %s\n", filename );
	sema.Release();
	return 0;
    }

    for (np = groups; np != NULL; np = np->next) {
        nrcFile.printf( "%s%c ", np->name, np->subscribed ? ':' : '!' );
	putReadList( &nrcFile, np->readList );
    }
    nrcFile.close();
    sema.Release();
    return 1;
}   // TNewsrc::writeFile



//--------------------------------------------------------------------------------



TNewsrc::pGroup TNewsrc::getGroupP( const char *groupName )
//
//  check, if groupName exists (return NULL, if not, otherwise pGroup)
//  "cache" is updated
//
{
    pGroup np;
    pGroup res;

#ifdef TRACE_ALL
    printfT( "TNewsrc::getGroupP(%s)\n", groupName );
#endif

    sema.Request();
    if (cacheGroupName == NULL  ||  stricmp(groupName,cacheGroupName) != 0) {
	int hasho;

	hasho = hashi(groupName,NEWSRC_HASHSIZE);
	
	for (np = hashTab[hasho];  np != NULL;  np = np->hashNext) {
	    if (stricmp(np->name, groupName) == 0) {
		cacheGroupName = np->name;
		cacheGroup     = np;
		break;
	    }
	}
	if (np == NULL) {
	    cacheGroupName = NULL;
	    cacheGroup     = NULL;
	}
    }
    res = cacheGroup;
    sema.Release();
#ifdef TRACE_ALL
    printfT( "TNewsrc::getGroupP(%s) = %p\n", groupName,cacheGroup );
#endif
    return res;
}   // TNewsrc::getGroupP



void TNewsrc::grpFixReadList( const char *groupName, long groupLo, long groupHi,
                              long initialCatchupCnt )
//
//  Sanity fixes to the read article number list
//
{
    pGroup np = getGroupP( groupName );
    Range *rp1, *rp2;

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpFixReadList(%s,%ld,%ld)\n",groupName,groupLo,groupHi );
    putReadList( NULL,np->readList );
#endif
    assert( np != NULL );
    sema.Request();

    if (np->initialCatchup) {
        np->initialCatchup = 0;
        if (initialCatchupCnt >= 0) {
            long lo;
            
            lo = groupHi - initialCatchupCnt + 1;
            if (lo < 1)
                lo = 1;
            if (lo > groupLo)
                groupLo = lo;
        }
    }

    //
    //  If the highest read article is greater than the highest
    //  available article, assume the group has been reset.
    //  But:  allow a small threshold due to cancelled articles (4)
    //
    if (np->readList != NULL) {
	for (rp1 = np->readList; rp1->next != NULL; rp1 = rp1->next)    // find end of list
	    ;
	if (rp1->hi > groupHi + ((groupHi > 100) ? 4 : 0)) {
	    //
	    //  delete all of the list
	    //
	    rp1 = np->readList;
	    while (rp1 != NULL) {
		rp2 = rp1->next;
		delete rp1;
		rp1 = rp2;
	    }
	    np->readList = NULL;
	}
    }

    //
    //  eliminate ranges lower than the lowest available article
    //  proceed from the beginning of the list...
    //
    rp1 = np->readList;
    while (rp1 != NULL  &&  groupLo > rp1->hi) {
#ifdef DEBUG_ALL
	printfT( "ellower: \n" );
#endif
	np->readList = rp1->next;
	delete rp1;
	rp1 = np->readList;
    }

    //
    //  All entries with a range below groupLo have been eliminated.  Also no entry of
    //  the list has a higher number than groupHi.  This means, that all entries are
    //  in the range of groupLo..groupHi (or the list is empty).
    //  If the list is empty, an entry from 1..groupLo-1 must be generated.  If the
    //  list is not empty and groupLo is smaller than rp1->lo again 1..groupLo-1 must
    //  be generated (if groupLo==1, nothing will be done)
    //
    rp1 = np->readList;
    if (rp1 == NULL  ||  groupLo < rp1->lo) {
	if (groupLo > 1) {
	    rp2 = new Range;
	    rp2->next = rp1;
	    rp2->lo = 1;
	    rp2->hi = groupLo-1;
	    np->readList = rp2;
	}
    }
    else if (rp1 != NULL)
	rp1->lo = 1;                 // old entry can be used!

#ifdef TRACE_ALL
    printfT( "grpFixReadList(): ende\n" );
    putReadList( NULL, np->readList );
#endif
    sema.Release();

    //
    //  fileChanged is not set!
    //
    return;
}   // TNewsrc::grpFixReadList



const char *TNewsrc::grpFirst( void )
//
//  return first subscribed group (NULL, if none)
//
{
    pGroup pp;

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpFirst()\n" );
#endif
    for (pp = groups;  pp != NULL;  pp = pp->next) {
#ifdef TRACE_ALL
	printfT( "TNewsrc::grpFirst(): %s,%d\n",pp->name,pp->subscribed );
#endif
	if (pp->subscribed)
	    return pp->name;
    }
    return NULL;
}   // TNewsrc::grpFirst



const char *TNewsrc::grpNext( const char *prevGroupName )
//
//
//  return next subscribed group (NULL, if none)
//
{
    pGroup np = getGroupP( prevGroupName );

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpNext(%s)\n",prevGroupName );
#endif
    assert( prevGroupName[0] != '\0' );
    assert( np != NULL );

    while (np != NULL) {
	np = np->next;
	if (np != NULL  &&  np->subscribed)
	    return np->name;
    }
    return NULL;
}   // TNewsrc::grpNext



int  TNewsrc::grpSubscribed( const char *groupName )
{
    pGroup np = getGroupP( groupName );

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpSubscribed(%s)\n",groupName );
#endif
    assert( np != NULL );
#ifdef TRACE_ALL
    printfT( "TNewsrc::grpSubscribed(%s): %d\n",np->name,np->subscribed );
#endif
    return np->subscribed;
}   // TNewsrc::grpSubscribed



int TNewsrc::grpExists( const char *groupName )
{
    pGroup np = getGroupP( groupName );

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpExists(%s)\n", groupName );
#endif

    return np != NULL;
}   // TNewsrc::grpExists



void TNewsrc::grpUnsubscribe( const char *groupName )
{
    pGroup np = getGroupP( groupName );

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpUnsubscibe(%s)\n",groupName );
#endif
    if (np != NULL) {
	np->subscribed = 0;
	fileChanged = 1;
    }
}   // TNewsrc::grpUnsubscribe



long TNewsrc::grpFirstUnread( const char *groupName, long groupLo )
//
//  Get first unread article number.
//
{
    pGroup np = getGroupP( groupName );
    long res;
    
#ifdef TRACE_ALL
    printfT( "TNewsrc::grpFirstUnread(%s,%ld)\n",groupName,groupLo );
#endif
    assert( np != NULL );

    sema.Request();
    if (np->readList == NULL)
	res = groupLo;
    else {
	if (groupLo < np->readList->lo)
	    res = groupLo;
	else
	    res = np->readList->hi + 1;
    }
    sema.Release();
    return res;
}   // TNewsrc::grpFirstUnread



int TNewsrc::artIsRead( const char *groupName, long artNum )
//
//  Determine if the article number has been read
//
{
    Range *head;
    pGroup np = getGroupP( groupName );

#ifdef TRACE_ALL
    printfT( "TNewsrc::artIsRead(%s,%ld)\n", groupName,artNum );
    putReadList( NULL,np->readList );
#endif
    assert( np != NULL );

    if (artNum <= 0)
	return 1;
    
    sema.Request();
    head = np->readList;
    
    //
    //  Look through the list
    //
    while (head != NULL) {
	if (artNum < head->lo) {
	    sema.Release();
	    return 0;
	}
	if (artNum >= head->lo  &&  artNum <= head->hi) {
	    sema.Release();
	    return 1;
	}
	head = head->next;
    }
    sema.Release();
    return 0;
}   // TNewsrc::artIsRead



void TNewsrc::artMarkRead( const char *groupName, long artNum )
//
//  Mark article as read.
//
{
    pGroup np = getGroupP( groupName );
    Range *rp, *trp, *lrp;

    if (np == NULL) {                // might be true for cross referencing
#ifdef TRACE_ALL
	printfT( "TNewsrc::artMarkRead(%s,%ld):  np==NULL\n",groupName,artNum );
#endif
	return;
    }
    if ( !np->subscribed)            // might be true for cross referencing
	return;

    //
    //  if article has been already read, do nothing...
    //
    if (artIsRead(groupName,artNum))
	return;
    
    fileChanged = 1;

#ifdef TRACE_ALL
    printfT( "TNewsrc::artMarkRead(%s,%ld)\n",groupName,artNum );
#endif

    sema.Request();
    rp = np->readList;

    /* If num is much lower than lowest range, or the list is
       empty, we need new entry */
    if (rp == NULL || artNum < rp->lo - 1) {
	trp = new Range;
	trp->lo = trp->hi = artNum;
	trp->next = rp;
	np->readList = trp;
	sema.Release();
	return;
    }

    /* lrp remembers last entry in case we need to add a new entry */
    lrp = NULL;

    /* Find appropriate entry for this number */
    while (rp != NULL) {
	/* Have to squeeze one in before this one? */
	if (artNum < rp->lo - 1) {
	    trp = new Range;
	    trp->lo = trp->hi = artNum;
	    trp->next = rp;
	    lrp->next = trp;
	    sema.Release();
	    return;
	}

	/* One less than entry's lo? */
	if (artNum == rp->lo - 1) {
	    rp->lo = artNum;
	    sema.Release();
	    return;
	}

	/* In middle of range, do nothing */
	if (artNum >= rp->lo && artNum <= rp->hi) {
	    sema.Release();
	    return;
	}

	/* One too high, must check if we merge with next entry */
	if (artNum == rp->hi + 1) {
	    if (rp->next != NULL && artNum == rp->next->lo - 1) {
		trp = rp->next;
		rp->hi = trp->hi;
		rp->next = trp->next;
		delete trp;
		sema.Release();
		return;
	    } else {
		/* No merge */
		rp->hi = artNum;
		sema.Release();
		return;
	    }
	}

	lrp = rp;
	rp = rp->next;
    }

    /* We flew off the end and need a new entry */
    trp = new Range;
    trp->lo = trp->hi = artNum;
    trp->next = NULL;
    lrp->next = trp;

#ifdef TRACE_ALL
    printfT( "TNewsrc::artMarkRead\n" );
#endif
    sema.Release();
    return;
}   // TNewsrc::artMarkRead



void TNewsrc::grpCatchup( const char *groupName, long groupLo, long groupHi, long numKeep )
{
    long lo;

#ifdef TRACE_ALL
    printfT( "TNewsrc::grpCatchup(%s,%ld,%ld,%ld)\n",groupName,groupLo,groupHi,numKeep );
#endif
    lo = groupHi - numKeep + 1;
    if (lo < 1)
	lo = 1;
    if (lo < groupLo)
	lo = groupLo;
    grpFixReadList( groupName, lo, groupHi );
    fileChanged = 1;
}   // TNewsrc::grpCatchup



void *TNewsrc::grpAdd( const char *groupName, int subscribe )
//
//  Adds new group name (in any case).  This means:  double entries are possible,
//  so check with grpExists() prior to the call to grpAdd()
//
{
    pGroup np;
    unsigned hasho;

    sema.Request();

    //
    //  allocate new entry
    //
    np = new Group;
    np->subscribed = subscribe;
    np->readList = NULL;
    np->name = xstrdup(groupName);
    np->next = NULL;
    np->initialCatchup = 0;

    //
    //  update hash list
    //
    hasho = hashi(groupName,NEWSRC_HASHSIZE);
    np->hashNext = hashTab[hasho];
    hashTab[hasho] = np;

    //
    //  add new group to end of list
    //
    if (groups == NULL)
	groups = np;
    else {
	pGroup pp = (addGroupP == NULL) ? groups : addGroupP;
	while (pp->next != NULL)
	    pp = pp->next;
	pp->next = np;
	addGroupP = np;
    }

    fileChanged = 1;

    sema.Release();
    return (void *)np;
}   // TNewsrc::grpAdd
