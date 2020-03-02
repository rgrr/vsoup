//  $Id: smtp.cc 1.29 1999/08/29 13:18:24 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  Send mail reply packet using SMTP directly
//


#include <ctype.h>
#include <string.h>

#include <rgfile.hh>
#include <rgmts.hh>
#include <rgsocket.hh>

#include "global.hh"
#include "smtp.hh"
#include "output.hh"
#include "util.hh"



void smtpClose(TSocket &socket)
//
//  close SMTP connection
//
{
#ifdef DEBUG
    printfT( "smtpClose(): QUIT\n" );
#endif
    socket.printf("QUIT\n");
    socket.close();
}   // smtpClose



static int getSmtpReply( TSocket &socket, const char *response)
//
//  get a response from the SMTP server and test it
//  on correct response a '1' is returned    
//
{
    char buf[BUFSIZ];

    do {
	buf[3] = '\0';
	if (socket.gets(buf, BUFSIZ) == NULL) {
	    areas.mailPrintf1( OW_XMIT, "Expecting SMTP %s reply, got nothing\n",
                               response, buf );
            areas.forceMail();
	    return 0;
	}
    } while (buf[3] == '-');		/* wait until not a continuation */

    if (strncmp(buf, response, 3) != 0) {
        areas.mailPrintf1( OW_XMIT, "Expecting SMTP %s reply, got %s\n", response, buf);
        areas.forceMail();
    }
    return (buf[0] == *response);	/* only first digit really matters */
}   // getSmtpReply



int smtpConnect ( TSocket &socket )
//
//  Open socket and intialize connection to SMTP server.
//  return value != 0  -->  ok
//
{
    const char *localhost;

    if (socket.open( smtpInfo.host,"smtp","tcp",smtpInfo.port ) < 0)
	return 0;

    if ( !getSmtpReply(socket, "220")) {
	areas.mailPrintf1( OW_XMIT,"Disconnecting from %s\n", smtpInfo.host);
	smtpClose(socket);
	return 0;
    }

    localhost = socket.getLocalhost();
    socket.printf("HELO %s\n", localhost );
#ifdef DEBUG
    printfT( "localhost: %s\n",localhost );
#endif
    delete localhost;

    if ( !getSmtpReply(socket, "250")) {
	areas.mailPrintf1( OW_XMIT,"Disconnecting from %s\n", smtpInfo.host);
	smtpClose(socket);
	return 0;
    }
    return 1;
}   // smtpConnect
    


static int sendSmtpRcpt( TSocket &socket, const char *buf, int *rcptFound )
//
//  Send RCPT command.
//
{
    int res;
    
    if (smtpEnvelopeOnlyDomain == NULL  ||
        (strlen(smtpEnvelopeOnlyDomain) <= strlen(buf)  &&
         stricmp(smtpEnvelopeOnlyDomain,buf+strlen(buf)-strlen(smtpEnvelopeOnlyDomain)) == 0)) {
        *rcptFound = 1;
        areas.mailPrintf1( OW_XMIT,"mailing to %s\n", buf);
        socket.printf( "RCPT TO:<%s>\n", buf );
        res = getSmtpReply(socket, "250");
        if ( !res) {
            areas.mailPrintf1( OW_XMIT,"recipient %s rejected\n", buf);
            areas.forceMail();
        }
        return res;
    }
    else
        return 1;
}   // sendSmtpRcpt



static int putAddresses( TSocket &socket, char *addresses, int *rcptFound )
//
//  Send an RCPT command for each address in the address list.
//
{
    const char *srcEnd;
    char *startAddr;
    char *endAddr;
    char saveCh;
    const char *addr;
    
    srcEnd = strchr(addresses, '\0');
    startAddr = addresses;

    while (startAddr < srcEnd) {
	endAddr = findAddressSep(startAddr);
	saveCh = *endAddr;
	*endAddr = '\0';
	addr = extractAddress(startAddr);
	if (addr) {
	    if ( !sendSmtpRcpt(socket, addr, rcptFound)) {
		delete addr;
		return 0;
	    }
	    delete addr;
	}
	*endAddr = saveCh;
	startAddr = endAddr + 1;
    }
    return 1;
}   // putAddresses



int smtpMail( TSocket &socket, TFile &file, size_t bytes)
//
//  Send message to SMTP server.
//  To all recipients the same message will be sent, i.e. Bcc is not handled
//  in a special way!  sendmail handles it the same way...
//
//  returns:
//  0  ->  failure
//  1  ->  transmission ok
//  2  ->  transmission not ok, but reconnect (file pointer positioned correctly)
//
{
    const  char *addr;
    char   buf[BUFSIZ];
    const  char *from;
    char   *resentTo;
    long   offset;
    size_t count;
    int    sol;                 // start of line
    int    ll;                  // line length
    int    inHeader;
    int    rcptFound;
    int    addrFailure = 0;
    int    bccHeader;
    int    xverUAfound;

    //
    //  Look for From: header
    //
    addr = NULL;
    from = getHeader( file, "From" );
    if (from != NULL)
        addr = extractAddress( from );

    //
    //  if there is no From:, check Sender: (OutLook!), if that fails
    //  either fail or generate dummy from (depends on smtpEnvelopeOnlyDomain).
    //
    if (from == NULL  ||  addr == NULL) {
        if (from != NULL)
            delete from;
        from = getHeader( file, "Sender" );
        if (from != NULL)
            addr = extractAddress( from );
        if (from == NULL  ||  addr == NULL) {
            if (from != NULL)
                delete from;
            if (smtpEnvelopeOnlyDomain) {
                from = xstrdup( "unknown" );
                addr = xstrdup( "unknown" );
            }
            else {
                areas.mailPrintf1( OW_XMIT,"no address in From header\n" );
                areas.forceMail();
                return 0;
            }
        }
    }

    //
    //  send MAIL command to SMTP server.
    //
    areas.mailPrintf1(OW_XMIT,"mailing from %s\n", addr);
    socket.printf("MAIL FROM:<%s>\n", addr);
    delete from;
    delete addr;
    
    if ( !getSmtpReply(socket, "250"))
        return 0;

    //
    //  find the destination!
    //
    rcptFound = 0;
    offset = file.tell();
    if (useReceived) {
        //
        //  FIRST CHANCE
        //  ============
        //  if '-R' has been specified check the 'Received:' header for the destination
        //  address and use it if found (this is for mail echo)
        //
        char *s, *dst, *t;
        s = (char *)getHeader( file,"Received" );
        dst = strstr( s," for <" );
        if ( !dst)
            dst = strstr( s,"\tfor <" );
        if (dst) {
            dst = (char *)xstrdup( dst+6 );
            t = strchr( dst,'>' );
            if (t)
                *t = '\0';
            putAddresses( socket, dst, &rcptFound );
        }
        else {
            dst = strstr( s," for " );
            if ( !dst)
                dst = strstr( s,"\tfor " );
            if (dst) {
                dst = (char *)xstrdup( dst+5 );
                t = strchr( dst,';' );
                if (t)
                    *t = '\0';
                putAddresses( socket, dst, &rcptFound );
            }
        }
    }
    if ( !rcptFound) {
        if ((resentTo = (char *)getHeader(file, "Resent-To")) != NULL) {
            //
            //  SECOND CHANCE
            //  =============
            //  Send to address on Resent-To header
            //  Continuation is allowed
            //
            if ( !putAddresses(socket, resentTo, &rcptFound))
                addrFailure = 1;
            delete resentTo;

            while (file.fgets(buf,sizeof(buf),1) != NULL) {
                if (buf[0] != ' ' && buf[0] != '\t')
                    break;
                if ( !putAddresses(socket, buf, &rcptFound))
                    addrFailure = 1;
            }
        }
        else {
            //
            //  THIRD CHANCE
            //  ============
            //  Send to addresses on To, Cc and Bcc headers.
            //
            int more = file.fgets(buf, sizeof(buf), 1) != NULL;
            while (more) {
                if (buf[0] == '\0')
                    break;

                if (isHeader(buf, "To")  ||
                    (includeCC  &&  (isHeader(buf, "Cc")  ||  isHeader(buf, "Bcc")))) {
                    //
                    //  first skip the To/Cc/Bcc field, then transmit the address
                    //
                    char *addrs;
                    for (addrs = buf;  *addrs != '\0' && !isspace(*addrs);  ++addrs)
                        ;
                    if ( !putAddresses(socket, addrs, &rcptFound))
                        addrFailure = 1;

                    //
                    //  Read next line and check if it is a continuation line.
                    //
                    while ((more = (file.fgets(buf,sizeof(buf),1) != NULL))) {
                        if (buf[0] != ' ' && buf[0] != '\t')
                            break;
                        if ( !putAddresses(socket, buf, &rcptFound))
                            addrFailure = 1;
                    }
		
                    continue;
                }
	
                more = file.fgets(buf, sizeof(buf), 1) != NULL;
            }
        }
    }
    //
    //  if there was no destination address specified and we are a mail echon, then
    //  send the mail to the POSTMASTER!
    //
    if (smtpEnvelopeOnlyDomain != NULL  &&  !rcptFound) {
        areas.mailPrintf1(OW_XMIT,"mailing to POSTMASTER\n");
        socket.printf( "RCPT TO:<postmaster%s%s>\n", buf,
                       (*smtpEnvelopeOnlyDomain == '@') ? "" : "@", smtpEnvelopeOnlyDomain );
        getSmtpReply(socket, "250");
    }

    /* Send the DATA command and the mail message line by line. */
    socket.printf("DATA\n");
    if ( !getSmtpReply(socket, "354"))
        addrFailure = 1;

    if (addrFailure) {
        const char *subject;

        file.seek(offset, SEEK_SET);
        subject = getHeader(file, "Subject");
        areas.mailPrintf1(OW_XMIT, "problem transmitting mail with subject\n        %s\n", subject );
        areas.forceMail();
        file.seek(offset+bytes, SEEK_SET);
	return 2;            // -> try to reconnect (but don't try to resend)!
    }

    file.seek(offset, SEEK_SET);
    count       = bytes;
    sol         = 1;            // start of line
    inHeader    = 1;
    bccHeader   = 0;
    xverUAfound = 0;
    while (file.fgets(buf, sizeof(buf)) != NULL  &&  count > 0) {
	//
	//  replace trailing "\r\n" with "\n"
	//
	ll  = strlen(buf);
	if (strcmp( buf+ll-2,"\r\n" ) == 0)
	    strcpy( buf+ll-2,"\n" );

        //
        //  header handling:
        //  - skip Bcc lines,
        //  - at the end of the Header add the VSoup version
        //
        if (inHeader  &&  sol) {
            if (isHeader(buf,"Bcc"))
                bccHeader = 1;
            else if (bccHeader)
                bccHeader = (buf[0] == ' '  ||  buf[0] == '\t');
            else if (isHeader(buf,XVER_UA0))
                xverUAfound = 1;
            else if (buf[0] == '\n') {
                inHeader  = 0;
                bccHeader = 0;
#if defined(XVER_UA0)
                socket.printf( "%s\n", !xverUAfound ? XVER_UA : XVER_NR );
#endif
            }
        }
        
	if (sol  &&  buf[0] == '.') {
	    //
	    //  is this a bug or a feature of SMTP?
	    //  the line "..\n" will be treated as EOA.
	    //  If there is a trailing blank, everything seems ok.
	    //
	    if (strcmp(buf,".\n") == 0)
		socket.printf( ".. \n" );
	    else
		socket.printf( ".%s",buf );
	}
        else {
            if ( !bccHeader)                   // dont transmit Bcc header!
                socket.printf( "%s",buf );
        }
	sol = (buf[strlen(buf)-1] == '\n');
	count -= ll;
    }
    file.seek(offset+bytes, SEEK_SET);

    socket.printf(".\n");
    if ( !getSmtpReply(socket, "250"))
	return 0;

    return 1;
}   // smtpMail
