//  $Id: kill.cc 1.22 1999/08/29 13:05:15 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  Kill file processing
//


#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regexp.h>
#include <unistd.h>
#include <time.h>

#include <rgmtreg.hh>
#include <rgmts.hh>

#include "kill.hh"
#include "nntp.hh"



//--------------------------------------------------------------------------------


class MOLines: public MatchObj {    //// warum ist hier public notwendig?
private:
    int greater;
    unsigned long lines;
public:
    MOLines( int agreater, unsigned long alines ) {
#ifdef DEBUG_ALL
	printfT("MOLines(%d,%lu)\n",agreater,alines);
#endif
	greater = agreater;  lines = alines;
    }
    ~MOLines() {}
    doesMatch( const char *line ) {
	unsigned long actline;
	return (sscanfT(line,"lines: %lu",&actline) == 1  &&
		(( greater  &&  actline > lines)    ||
		 (!greater  &&  actline < lines)));
    }
};


class MODate: public MatchObj {
private:
    int greater;
    long ddays;
    time_t now;
public:
    MODate( int agreater, unsigned long addays ) {
#ifdef DEBUG_ALL
	printfT("MODate(%d,%lu)\n",agreater,addays);
#endif
	greater = agreater;  ddays = addays;
	time( &now );
    }
    ~MODate() {}
    doesMatch( const char *line ) {
	const char *s, *w[10];
	int inword;
	int maxword;
	const char *day, *month, *year;
	int nday, nmonth, nyear;
	const char *monthname[] = {"jan","feb","mar","apr","may","jun",
				   "jul","aug","sep","oct","nov","dec"};
	struct tm tmArtTime;
	time_t artTime;
	long diffDays;
	
	if (strncmp(line,"date: ",6) != 0)
	    return 0;
#ifdef DEBUG_ALL
	printfT("MODate::doesMatch(): %s\n",line+6);
#endif
	//
	//  find the the beginning of each word (and write the result to w[])
	//
	inword = 0;
	maxword = 0;
	for (s=line+6;  *s!='\0' && maxword < 9;  ++s) {
	    if (*s != ' ') {
		if ( !inword) {
		    w[maxword++] = s;
		    inword = 1;
		}
	    }
	    else
		inword = 0;
	}
	if (maxword < 4)    // at least four words required
	    return 0;

	if (isalpha(*w[0])  &&  isdigit(*w[1])) {
	    //
	    //  date format:  Wdy, DD Mon YY[YY] HH:MM[:SS] [Timezone]
	    //
	    day   = w[1];
	    month = w[2];
	    year  = w[3];
	}
	else if (isdigit(*w[0])  &&  isalpha(*w[1])) {
	    //
	    //  date format:  DD Mon YY[YY] HH:MM[:SS] [Timezone]
	    //
	    day   = w[0];
	    month = w[1];
	    year  = w[2];
	}
	else if (strchr(w[2],',') != NULL) {
	    //
	    //  date format: Wdy Mon DD, YYYY
	    //
	    day   = w[2];
	    month = w[1];
	    year  = w[3];
	}
	else {
	    //
	    //  date format: Wdy Mon DD HH:MM:SS [Timezone] YYYY
	    //
	    day   = w[2];
	    month = w[1];
	    year  = w[maxword-1];
	}
	for (nmonth = 0;  nmonth < 12;  ++nmonth) {
	    if (strncmp(monthname[nmonth], month, 3) == 0)
		break;
	}
	++nmonth;
	nday  = atoi(day);
	nyear = atoi(year);
	if (nyear < 80)
	    nyear += 2000;
	else if (nyear < 100)
	    nyear += 1900;
#ifdef DEBUG_ALL
	printfT( "MODate::doesMatch()::: %d.%d.%d\n", nday, nmonth, nyear );
#endif
	if (nmonth <    1  ||  nmonth >   12  ||
	    nyear  < 1970  ||   nyear > 2100  ||    // little bit optimistic (but who ever knows...)
	    nday   <    1  ||    nday >   31)
	    return 0;

	tmArtTime.tm_sec   = 0;
	tmArtTime.tm_min   = 0;
	tmArtTime.tm_hour  = 0;
	tmArtTime.tm_mday  = nday;
	tmArtTime.tm_mon   = nmonth-1;
	tmArtTime.tm_year  = nyear-1900;
	tmArtTime.tm_wday  = 0;
	tmArtTime.tm_yday  = 0;
	tmArtTime.tm_isdst = 0;
	artTime = mktime( &tmArtTime );

	if (artTime > now)     // is possible due to timezones...
	    return 0;
	//
	//  I'm ignoring silently, that there is a difftime (I don't like floats)
	//  Also time_t is unsigned long which means overflow will occur around 2100..
	//  Rounding is also not done, because it is meaningless, because TZ is ignored.
	//
	//diffDays = (long)((difftime(now,artTime)) / 86400.0);
	diffDays = (now-artTime) / 86400;
#ifdef DEBUG_ALL
	printfT( "MODate::doesMatch()---- %ld\n", diffDays );
#endif
	return  ( greater  &&  diffDays > ddays)    ||
		(!greater  &&  diffDays < ddays);
    }
};


class MOPatternAll: public MatchObj {
private:
    regexp *re;
public:
    MOPatternAll( const char *exp ) {
#ifdef DEBUG_ALL
	printfT( "MOPatternAll(%s)\n",exp );
#endif
	re = regcompT( exp );
    }
    ~MOPatternAll() { delete re; }
    doesMatch( const char *line ) { return regexecT( re,line ); }
};


class MOPattern: public MatchObj {
private:
    char *where;
    int wherelen;
    regexp *re;
public:
    MOPattern( const char *awhere, const char *exp ) {
#ifdef DEBUG_ALL
	printfT( "MOPattern(%s,%s)\n",awhere,exp );
#endif
	where = new char [strlen(awhere)+2];
	sprintfT( where,"%s:",awhere );
	wherelen = strlen( where );
	re = regcompT( exp );
    }
    ~MOPattern() { delete where;  delete re; };
    doesMatch( const char *line ) {
	return (strncmp(line,where,wherelen) == 0  &&
		regexecT( re,line+wherelen ));
    }
};


class MOStringAll: public MatchObj {
private:
    const char *s;
public:
    MOStringAll( const char *as ) {
#ifdef DEBUG_ALL
	printfT( "MOStringAll(%s)\n",as );
#endif
	s = xstrdup( as );
    }
    ~MOStringAll() { delete s; }
    doesMatch( const char *line ) { return strstr(line,s) != NULL; }
};


class MOString: public MatchObj {
private:
    char *where;
    int wherelen;
    const char *s;
public:
    MOString( const char *awhere, const char *as ) {
#ifdef DEBUG_ALL
	printfT( "MOString(%s,%s)\n",awhere,as );
#endif
	where = new char [strlen(awhere)+2];
	sprintfT( where,"%s:",awhere );
	wherelen = strlen(where);
	s = xstrdup( as );
    }
    ~MOString() { delete where;  delete s; }
    doesMatch( const char *line ) {
	return (strncmp(line,where,wherelen) == 0  &&
		strstr(line+wherelen,s) != NULL);
    }
};


//--------------------------------------------------------------------------------



TKillFile::TKillFile( void )
{
    groupKillList = actGroupList = NULL;
    actGroupName = xstrdup("");
    killThreshold = 0;
}   // TKillFile::TKillFile



void TKillFile::killGroup( Group *gp )
{
    Exp *ep1, *ep2;
    
    if (gp == NULL)
	return;

    ep1 = gp->expList;
    while (ep1 != NULL) {
	if (ep1->macho != NULL) {
	    delete ep1->macho;
	}
	ep2 = ep1->next;
	delete ep1;
	ep1 = ep2;
    }
    delete gp->grpPat;
    delete gp;
}   // TKillFile::killGroup



TKillFile::~TKillFile()
{
    Group *gp1, *gp2;

    gp1 = groupKillList;
    while (gp1 != NULL) {
	gp2 = gp1->next;
	killGroup( gp1 );
	gp1 = gp2;
    }

    gp1 = actGroupList;
    while (gp1 != NULL) {
	gp2 = gp1->next;
	delete gp1;
	gp1 = gp2;
    }
    delete actGroupName;
}   // TKillFile::~TKillFile



void TKillFile::stripBlanks( char *line )
{
    char *p1, *p2;
    int  len;

    p1 = line;
    while (*p1 == ' '  ||  *p1 == '\t')
	++p1;
    p2 = line + strlen(line) - 1;
    while (p2 >= p1  &&  (*p2 == ' '  ||  *p2 == '\t'))
	--p2;
    len = p2-p1+1;
    if (len > 0) {
	memmove( line,p1,len );
	line[len] = '\0';
    }
    else
	line[0] = '\0';
}   // TKillFile::stripBlanks



int TKillFile::readLine( char *line, int n, TFile &inf, int &lineNum )
//
//  fetch the next line from file
//  blanks are stripped
//  blank lines & lines with '#' in the beginning are skipped
//  on EOF NULL is returned
//    
{
    for (;;) {
	*line = '\0';
	if (inf.fgets(line,n,1) == NULL)
	    return 0;
	++lineNum;
	stripBlanks( line );
	if (line[0] != '\0'  &&  line[0] != '#')
	    break;
    }
    return 1;
}   // TKillFile::readLine



int TKillFile::readFile( const char *killFile )
//
//  Read kill file and compile regular expressions.
//  Return:  -1 -> file not found, 0 -> syntax error, 1 -> ok
//  Nicht so hanz das optimale:  besser w„re es eine Zustandsmaschine
//  zusammenzubasteln...
//
{
    char buf[1000], name[1000], tmp[1000];
    char searchIn[1000], searchFor[1000];
    TFile inf;
    Group *pGroup, *pLastGroup;
    Exp *pLastExp;
    char ok;
    int lineNum;

    groupKillList = NULL;

    if ( !inf.open(killFile,TFile::mread,TFile::otext))
	return -1;

    sema.Request();

    pLastGroup = NULL;
    ok = 1;

    //
    //  read newsgroup name
    //
    killThreshold = 0;
    lineNum = 0;
    while (ok  &&  readLine(buf,sizeof(buf),inf,lineNum)) {
#ifdef DEBUG_ALL
	printfT( "line: '%s'\n",buf );
#endif
	//
	//  check for "killthreshold"-line
	//
	{
	    long tmp;
	    
	    if (sscanfT(buf,"killthreshold %ld",&tmp) == 1) {
		killThreshold = tmp;
		continue;
	    }
	}
	
	//
	//  check for "quit"-line
	//
	if (strcmp(buf,"quit") == 0)
	    break;
	
	//
	//  check for: <killgroup> "{"
	//
	if (sscanfT(buf,"%s%s",name,tmp) == 1) {
	    if ( !readLine(tmp,sizeof(tmp),inf,lineNum)) {
		ok = 0;
		break;
	    }
	}
	if (tmp[0] != '{' || tmp[1] != '\0') {
	    ok = 0;
	    break;
	}

	//
	//  create Group-List entry and append it to groupKillList
	//
	strlwr( name );
	if (stricmp(name, "all") == 0)
	    strcpy( name,".*" );               // use 'special' pattern which matches all group names
	pGroup = new Group;
	pGroup->grpPat = regcompT( name );
	pGroup->expList = NULL;
	pGroup->next = NULL;

	if (pLastGroup == NULL)
	    groupKillList = pGroup;
	else
	    pLastGroup->next = pGroup;
	pLastGroup = pGroup;

	//
	//  Read kill expressions until closing brace.
	//
	pLastExp = NULL;
	while (readLine(buf,sizeof(buf),inf,lineNum)) {
	    long points;
	    unsigned long number;
	    MatchObj *macho = NULL;
	    Exp *pExp;

	    strlwr( buf );
#ifdef DEBUG_ALL
	    printfT( "TKillfile::readFile(): '%s'\n",buf );
#endif
	    if (buf[0] == '}'  &&  buf[1] == '\0') {
#ifdef DEBUG_ALL
		printfT( "TKillfile::readFile(): end of group\n" );
#endif
		break;
	    }

            //
            //  due to emxfix01 the optional ':' is a real stopper...
            //
	    *searchIn = *searchFor = '\0';
            if (sscanfT(buf,"%ld pattern header %[^\n]", &points,searchFor) == 2  ||
                sscanfT(buf,"%ld pattern header: %[^\n]", &points,searchFor) == 2) {
		macho = new MOPatternAll( searchFor );
	    }
            else if (sscanfT(buf,"%ld pattern %[^ \t:] %[^\n]", &points, searchIn, searchFor) == 3  ||
                     sscanfT(buf,"%ld pattern: %[^ \t:] %[^\n]", &points, searchIn, searchFor) == 3) {
		macho = new MOPattern( searchIn,searchFor );
	    }
            else if (sscanfT(buf,"%ld header %[^\n]", &points,searchFor) == 2  ||
                     sscanfT(buf,"%ld header: %[^\n]", &points,searchFor) == 2) {
		macho = new MOStringAll( searchFor );
	    }
            else if (sscanfT(buf,"%ld date %[<>] %lu", &points,searchFor,&number) == 3  ||
                     sscanfT(buf,"%ld date: %[<>] %lu", &points,searchFor,&number) == 3) {
		if (searchFor[1] != '\0') {
		    ok = 0;
		    break;
		}
		macho = new MODate( *searchFor == '>', number );
	    }
            else if (sscanfT(buf,"%ld lines %[<>] %lu", &points,searchFor,&number) == 3  ||
                     sscanfT(buf,"%ld lines: %[<>] %lu", &points,searchFor,&number) == 3) {
		if (searchFor[1] != '\0') {
		    ok = 0;
		    break;
		}
		macho = new MOLines( *searchFor == '>', number );
	    }
            else if (sscanfT(buf,"%ld %[^ \t:] %[^\n]", &points,searchIn,searchFor) == 3  ||
                     sscanfT(buf,"%ld %[^ \t:]: %[^\n]", &points,searchIn,searchFor) == 3) {
		macho = new MOString( searchIn,searchFor );
	    }
	    else {
		ok = 0;
		break;
	    }
	    
	    assert( macho != NULL );
	    
	    //
	    //  create entry and append it to expression list
	    //
	    pExp = new Exp;
	    pExp->points = points;
	    pExp->macho  = macho;
	    pExp->next   = NULL;
	    if (pLastExp == NULL)
		pGroup->expList = pExp;
	    else
		pLastExp->next = pExp;
	    pLastExp = pExp;
	}
    }
    sema.Release();

    inf.close();

    if ( !ok)
	hprintfT( STDERR_FILENO, "error in score file %s,\n        section %s, line %d\n",
		  killFile,name,lineNum);
    return ok;
}   // TKillFile::readFile



TKillFile::Group *TKillFile::buildActGroupList( const char *groupName )
//
//  return group kill for *groupName
//  requires semaphor protection, because it is *NOT* reentrant (there is only one list)
//
{
    Group *p;
    Group **pp;
    char *name;

#ifdef TRACE_ALL
    printfT( "TKillFile::buildActGroupList(%s)\n",groupName );
#endif

    name = (char *)xstrdup( groupName );
    strlwr( name );

    if (stricmp(name,actGroupName) != 0) {
	pp = &actGroupList;
	for (p = groupKillList; p != NULL; p = p->next) {
	    //
	    //  is groupname matched by a killgroup regexp?
	    //
	    if (regexecT(p->grpPat,name)) {
		//
		//  does the killgroup regexp match the complete groupname?
		//
		if (name              == p->grpPat->startp[0]  &&
		    name+strlen(name) == p->grpPat->endp[0]    &&
		    p->expList        != NULL) {    // expList ist Endekennung.  Deshalb darf eine leere Gruppe nicht in die Liste aufgenommen werden (rg290597)
#ifdef DEBUG_ALL
		    printfT( "regexec: %p,%ld %p %p\n", name,strlen(name), p->grpPat->startp[0], p->grpPat->endp[0] );
#endif
		    if (*pp == NULL) {
			*pp = new Group;
			(*pp)->next = NULL;
		    }
		    (*pp)->expList = p->expList;
		    pp = &((*pp)->next);
		}
	    }
	}
	if (*pp != NULL)
	    (*pp)->expList = NULL;
	xstrdup( &actGroupName, name );
    }

    delete name;
    return actGroupList;
}   // TKillFile::buildActGroupList



long TKillFile::matchLine( const char *agroupName, const char *aline )
//
//  Check if line matches score criteria (line must be already in lower case!)
//  Return the sum of the matching scores.
//
{
    Group *pGroup;
    Exp   *pExp;
    char groupName[NNTP_STRLEN];
    char line[NNTP_STRLEN];
    long points;

    if (agroupName == NULL  ||  aline == NULL)
	return killThreshold;

    strcpy( groupName,agroupName );  strlwr( groupName );
    strcpy( line, aline );           strlwr( line );

    sema.Request();

    buildActGroupList( groupName );
    points = 0;
    for (pGroup = actGroupList;
	 pGroup != NULL  &&  pGroup->expList != NULL;
	 pGroup = pGroup->next) {
        for (pExp = pGroup->expList; pExp != NULL; pExp = pExp->next) {
            if (pExp->macho->doesMatch(line)) {
#ifdef DEBUG_ALL
                printfT( "TKillFile::matchLine(): %s,%ld\n",line,pExp->points );
#endif
		points += pExp->points;
	    }
	}
    }
#ifdef DEBUG_ALL
    if (points < 0)
	printfT( "killit\n" );
#endif

    sema.Release();

    return points;
}   // TKillFile::matchLine



int TKillFile::doKillQ( const char *groupName )
{
    Group *p;
    
    sema.Request();
    p = buildActGroupList(groupName);
    sema.Release();
    return p != NULL  &&  p->expList != NULL;    // minimum one match
}   // doKillQ
