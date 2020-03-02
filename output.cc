//  $Id: output.cc 1.4 1999/08/29 13:11:08 Hardy Exp Hardy $
//
//  This progam/module was written by Hardy Griech based on ideas and
//  pieces of code from Chin Huang (cthuang@io.org).  Bug reports should
//  be submitted to rgriech@swol.de.
//
//  This file is part of VSoup for OS/2.  VSoup including this file
//  is freeware.  There is no warranty of any kind implied.  The terms
//  of the GNU Gernal Public Licence are valid for this piece of software.
//
//  Output data to console and logfile
//

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/winmgr.h>

#include <rgmts.hh>

#include "global.hh"
#include "output.hh"


static TSemaphor sysSema;

static int scrsiz[2];
static wm_handle h[OW_MAX+1];
static int outputInitialized = 0;
static int wmLastLF[OW_MAX+1];            // cursor is at leftmost position
static int wmWidth[OW_MAX+1];             // -1 -> win not initialized

#define SCRX (scrsiz[0])
#define SCRY (scrsiz[1])



int OutputInitialized( int wn )
//
// return -1 if window 'system' has not been initialized,
// otherwise a window to which output can be (re)directed
//
{
    int res = -1;
    
    if ( !outputInitialized)
        res = -1;
    else if (wn >= 0  &&  wn <= OW_MAX  &&  wmWidth[wn] >= 0)
        res = wn;
    else if (wmWidth[OW_NEWS] >= 0)
        res = OW_NEWS;
    else if (wmWidth[OW_MAIL] >= 0)
        res = OW_MAIL;
    else if (wmWidth[OW_XMIT] >= 0)
        res = OW_XMIT;
    else
        res = -1;
    return res;
} // OutputInitialized



void OutputInit( void )
/*
 * global init of windowed output
 */
{
    int i;
    int wmHeight;

    if ( !outputInitialized) {
        for (i = 0;  i <= OW_MAX;  ++i)
            wmWidth[i] = -1;                // -> !OUTPUTINITIALIZED(i)
    
        _scrsize( scrsiz );

#ifndef DEBUG
        wm_init( OW_MAX+1 );

        outputInitialized = 1;
    
        //
        // create OW_MISC
        //
        h[OW_MISC] = wm_create( 1,1,SCRX-2,1, 2,0, B_BLUE | F_WHITE );
        wm_dimen( h[OW_MISC], wmWidth+OW_MISC, &wmHeight );
        wm_open( h[OW_MISC] );
        wm_wrap( h[OW_MISC], 0 );
        wmLastLF[OW_MISC] = 1;
        wm_border( h[OW_MISC], 2, B_BLUE | F_WHITE, "", 0, B_CYAN | F_WHITE );
#endif
    }
}   // OutputInit



void OutputExit( void )
{
    int i;
    
    if (outputInitialized) {
        //wm_close_all();         // stellt alten Fensterzustand wieder her (ist das erwÅnscht?)

        for (i = 0;  i <= OW_MAX;  ++i) {
            if (OutputInitialized(i) == i)
                wm_delete( h[i] );
        }

        wm_exit();
    }
}   // OutputExit



void OutputOpenWindows( int winMask, const char *newsTitle, const char *mailTitle, const char *xmitTitle )
//
// open OW_NEWS, OW_MAIL, OW_XMIT
// OW_MISC has already been created during OutputInit...
//
{
    int wcnt;
    int i;
    int wmHeight;
    int w1, w2;
    
    if (outputInitialized) {
        wcnt = ((winMask & (1 << OW_NEWS)) ? 1 : 0) +
            ((winMask & (1 << OW_MAIL)) ? 1 : 0) +
            ((winMask & (1 << OW_XMIT)) ? 1 : 0);

        w1 = 0;     // only for GCC...
        w2 = 0;
        if (wcnt == 1) {
            if (winMask & (1 << OW_NEWS))
                w1 = OW_NEWS;
            else if (winMask & (1 << OW_MAIL))
                w1 = OW_MAIL;
            else if (winMask & (1 << OW_XMIT))
                w1 = OW_XMIT;
            h[w1] = wm_create( 1,4, SCRX-2,SCRY-2, 2,0, B_BLUE | F_WHITE );
            wm_dimen( h[w1], wmWidth+w1, &wmHeight );
        }
        else if (wcnt == 2) {
            if (winMask & (1 << OW_NEWS))
                w2 = OW_NEWS;
            if (winMask & (1 << OW_MAIL)) {
                w1 = w2;
                w2 = OW_MAIL;
            }
            if (winMask & (1 << OW_XMIT)) {
                w1 = w2;
                w2 = OW_XMIT;
            }
            h[w1] = wm_create( 1,4, SCRX-2,1*SCRY/2, 2,0, B_BLUE | F_WHITE );
            wm_dimen( h[w1], wmWidth+w1, &wmHeight );
            h[w2] = wm_create( 1,1*SCRY/2+3, SCRX-2,SCRY-2, 2,0, B_BLUE | F_WHITE );
            wm_dimen( h[w2], wmWidth+w2, &wmHeight );
        }
        else /* if (wcnt == 3) */ {
            h[OW_NEWS] = wm_create( 1,4, SCRX/2-2,SCRY-2, 2,0, B_BLUE | F_WHITE );
            wm_dimen( h[OW_NEWS], wmWidth+OW_NEWS, &wmHeight );
            h[OW_MAIL] = wm_create( SCRX/2+1,0*SCRY/2+4, SCRX-2,1*SCRY/2, 2, 0, B_BLUE | F_WHITE );
            wm_dimen( h[OW_MAIL], wmWidth+OW_MAIL, &wmHeight );
            h[OW_XMIT] = wm_create( SCRX/2+1,1*SCRY/2+3, SCRX-2,2*SCRY/2-2, 2, 0, B_BLUE | F_WHITE );
            wm_dimen( h[OW_XMIT], wmWidth+OW_XMIT, &wmHeight );
        }

        for (i = 0;  i <= OW_MAX;  ++i) {
            if (i != OW_MISC  &&  OutputInitialized(i) == i) {
                if (i == OW_NEWS)
                    wm_border( h[OW_NEWS], 2, B_BLUE | F_CYAN, newsTitle ? newsTitle : " News Reception ", 0, B_CYAN | F_WHITE );
                else if (i == OW_MAIL)
                    wm_border( h[OW_MAIL], 2, B_BLUE | F_GREEN, mailTitle ? mailTitle : " Mail Reception ", 0, B_GREEN | F_BLACK);
                else /* if (i == OW_XMIT) */
                    wm_border( h[OW_XMIT], 2, B_BLUE | F_MAGENTA, xmitTitle ? xmitTitle : " Transmission ", 0, B_MAGENTA | F_WHITE );
                wm_open( h[i] );
                wm_wrap( h[i], 0 );
                wmLastLF[i] = 1;
            }
        }
    }
}   // OutputOpenWindows



void Output( int wn, const char *fmt, ... )
{
    va_list ap;
    
    va_start( ap, fmt );
    OutputV( wn, fmt, ap );
    va_end( ap );
}   // Output



void OutputV( int wn, const char *fmt, va_list arg_ptr )
{
    //
    //  Send a formatted string to the output device
    //  For windowed output the string will be somehow formatted, if the last character sent to the
    //  window was a '\n'.
    //  Recommendation:  do not break a single line into several calls to OutputV()!!!
    //
    char buf[BUFSIZ];
    char *s, *t;
    int lastIsLF;
    int outMaxWidth;
    int outWidth;
    int first;
    
    vsnprintfT( buf, sizeof(buf), fmt, arg_ptr );

    //
    //  on empty string return immediately
    //
    if (*buf == '\0')
        return;

    sysSema.Request();
        
    //
    //  check, if last character is a LF (CR)
    //
    lastIsLF = (buf[strlen(buf)-1] == '\n');
    
    if ((wn = OutputInitialized(wn)) != -1) {
        //
        // windowed output
        //
        if (buf[0] == '\n'  &&  buf[1] == '\0')
            wm_putc( h[wn], '\n' );     // output a single LF (otherwise not possible!)
        else if (wmLastLF[wn]) {
            //
            // replace "\r\t\n" by ' '
            //
            for (s = strchr(buf,'\t');  s != NULL;  s = strchr(s+1,'\t'))
                *s = ' ';
            for (s = strchr(buf,'\r');  s != NULL;  s = strchr(s+1,'\r'))
                *s = ' ';
            for (s = strchr(buf,'\n');  s != NULL;  s = strchr(s+1,'\n'))
                *s = ' ';

            //
            // remove double occurances of blanks
            // leading blanks are allowed...
            //
            for (s = buf;  *s == ' ';  ++s)
                ;
            t = s;
            while (*s != '\0') {
                if (*s != ' ')
                    *(t++) = *(s++);
                else {
                    *(t++) = *(s++);
                    while (*s == ' ')
                        ++s;
                }
            }
            //
            // remove trailing blanks
            //
            *t = '\0';
            while (t > buf  &&  *(--t) == ' ')
                *t = '\0';

            //
            // output a maximum of outMaxWidth characters per line
            //
            s = buf;
            first = 1;
            outMaxWidth = wmWidth[wn];
            while (strlen(s) > (unsigned)outMaxWidth) {
                //
                // if line's too long...
                //
                // look for last blank in line and use it as line break
                // if there is no blank, use a ',' instead
                // if there is also no ',', use a '.' instead
                // if there is none, output OutMaxWidth characters...
                //
                for (outWidth = outMaxWidth;  s[outWidth] != ' '  &&  outWidth > 0;  --outWidth)
                    ;
                if (outWidth == 0) {
                    for (outWidth = outMaxWidth;  s[outWidth-1] != ','  &&  outWidth > 0;  --outWidth)
                        ;
                    if (outWidth == 0) {
                        for (outWidth = outMaxWidth;  s[outWidth-1] != '.'  &&  outWidth > 0;  --outWidth)
                            ;
                        if (outWidth == 0)
                            outWidth = outMaxWidth;
                    }
                }
                else
                    ++outWidth;             // auto-line wrapping -> the trailing blank will not be displayed

                if (outWidth >= outMaxWidth)
                    wm_printf( h[wn], "%s%.*s\n", first?"":"       ",outWidth, s );
                else {
                    wm_printf( h[wn], "%s%.*s", first?"":"       ",outWidth, s );
                    OutputClrEol( wn );         // clreol for wrapped lines (because they might shorter then forseen)
                    wm_printf( h[wn], "\n" );
                }
                
                s += outWidth;
                if (first) {
                    first = 0;
                    outMaxWidth = outMaxWidth - 7;
                }
            }
            //
            // remainder of line:
            // - suppress empty lines
            // - if there was a '\n' appended, output it now
            //
            if (*s != '\0') {
                wm_printf( h[wn],"%s%s", first?"":"       ", s );
                if (lastIsLF)
                    wm_printf( h[wn], "\n" );
            }
        }
        else
            wm_printf( h[wn], "%s", buf );   // this should not happen
    }
    else {
        //
        //  output to stdout
        //
        printfT( "%s",buf );
    }
    
    wmLastLF[wn] = lastIsLF;

    sysSema.Release();
}   // OutputV



void OutputClrEol( int wn )
{
    if ((wn = OutputInitialized(wn)) != -1) {
        wm_clr_eol( h[wn], wm_getx(h[wn]), wm_gety(h[wn]) );
    }
    else {
        printfT( "%-70s\r","" );
    }
}   // OutputClrEol



void OutputCR( int wn )
{
    if ((wn = OutputInitialized(wn)) != -1) {
        wm_gotoxy( h[wn], 0, wm_gety(h[wn]) );
    }
    else {
        printfT( "\r" );
    }
    wmLastLF[wn] = 1;
}   // OutputCR
