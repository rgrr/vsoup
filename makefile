# $Id: makefile 1.28 1999/08/29 13:46:49 Hardy Exp Hardy $
#
# Big problems with library and general making:
# - required lib modules should be distributed,
# - one makefile for rg and other world.
# Trick:  if RGLIB will be defined empty (e.g. 'make RGLIB=""' or 'dmake RGLIB=""'
#         make will go-on without generation of rglib.a (e.g. for my environment)

#
# OS/2 CFLAGS follows (second: debugging):
#
CFLAGS = -DOS2 -DOS2EMX_PLAIN_CHAR -Wall -I. -O1 -DNDEBUG -Zmt
#CFLAGS = -DOS2 -DOS2EMX_PLAIN_CHAR -Wall -I. -g -DTRACE -DTRACE_ALL -DDEBUG -DDEBUG_ALL -Zmtd
#CFLAGS = -DOS2 -DOS2EMX_PLAIN_CHAR -Wall -I. -g -DTRACE -DDEBUG -Zmtd
#
# WIN32 CFLAGS follows (second: debugging):
#
#CFLAGS = -Zsys -Zmt -Zwin32 -Wall -I. -DHANDLEERR -DNDEBUG -O1
#CFLAGS = -Zsys -Zmt -Zwin32 -Wall -I. -DHANDLEERR -DDEBUG -DTRACE -DDEBUG_ALL -DTRACE_ALL

AR      = ar
CC      = gcc
O       = .o
A       = .a
LDFLAGS = 
PROGRAM = vsoup.exe
VERSION = 129
MISC    = d:/b/32/yarnio.cmd d:/b/32/yarnio.set d:/b/32/modifyemxh.cmd \
          d:/b/32/loginisp.cmd d:/b/32/logoutisp.cmd \
          file_id.diz rmhigh.cmd

RGPATH  = d:/u/c/lib/
RGSRC   = $(RGPATH)rgsema.hh \
          $(RGPATH)rgfile.hh   $(RGPATH)rgfile.cc \
          $(RGPATH)rgmts.hh    $(RGPATH)rgmts.cc \
          $(RGPATH)rgmtreg.hh  $(RGPATH)rgmtreg.cc \
          $(RGPATH)rgsocket.hh $(RGPATH)rgsocket.cc \
          $(RGPATH)md5.hh      $(RGPATH)md5.cc
RGOBJ   = rgfile$(O) rgmtreg$(O) rgmts$(O) rgsocket$(O) md5$(O)
RGLIB   = rglib$(A)

OBJECTS = areas$(O) kill$(O) main$(O) news$(O) newsrc$(O) nntpcl$(O) pop3$(O) \
          reply$(O) smtp$(O) util$(O) output$(O)
LIBS    = -lrglib -lregexp -lsocket -lstdcpp -lvideo

.cc.o:
	$(CC) $(CFLAGS) -c $<


$(PROGRAM): $(OBJECTS) $(RGLIB)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $(PROGRAM) $(OBJECTS) $(LIBS) -L.

rglib.a: $(RGOBJ)
	$(AR) r $(RGLIB) $(RGOBJ)

doc:    vsoup.inf

vsoup.inf: vsoup.src
	emxdoc -t -b1 -c -o vsoup.txt vsoup.src
	emxdoc -i -c -o vsoup.ipf vsoup.src
	ipfc /inf vsoup.ipf
	del vsoup.ipf

convsoup.exe: convsoup.o
	$(CC) $(LDFLAGS)  $(CFLAGS) -o convsoup.exe convsoup.o

ownsoup.exe:  ownsoup.o
	$(CC) $(LDFLAGS)  $(CFLAGS) -o ownsoup.exe ownsoup.o -lregexp

qsoup.exe:    qsoup.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o qsoup.exe qsoup.o

zipsrc:
	-del vsrc$(VERSION).zip > nul 2>&1
	zip -9j vsrc$(VERSION) *.cc *.hh makefile $(RGSRC)
	uuencode vsrc$(VERSION).zip vsrc$(VERSION).uue

zip:    $(PROGRAM) convsoup.exe ownsoup.exe qsoup.exe doc zipsrc
	emxbind -s $(PROGRAM)
	-del vsoup$(VERSION).zip  > nul 2>&1
	zip -9j vsoup$(VERSION) readme.1st copying vsrc$(VERSION).zip *.ico vsoup.txt vsoup.inf $(MISC) $(PROGRAM) convsoup.exe ownsoup.exe qsoup.exe
	uuencode vsoup$(VERSION).zip vsoup$(VERSION).uue

clean:
	-del *.o         > nul 2>&1
	-del *.a         > nul 2>&1
	-del *.obj       > nul 2>&1
	-del *.exe       > nul 2>&1
	-del *~          > nul 2>&1
	-del *.ipf       > nul 2>&1

depend:
	awk "{ if ($$0 ~ /^# Dependency list follows/) {exit} else {print}}" makefile > makefile.tmp
	echo # Dependency list follows   >> Makefile.tmp
	gcc -MM *.cc >> Makefile.tmp && mv Makefile.tmp Makefile

# Dependency list follows   
areas.o: areas.cc areas.hh output.hh
convsoup.o: convsoup.cc
kill.o: kill.cc kill.hh nntp.hh
main.o: main.cc areas.hh global.hh newsrc.hh news.hh pop3.hh reply.hh \
 util.hh output.hh
news.o: news.cc areas.hh global.hh newsrc.hh kill.hh news.hh nntp.hh \
 nntpcl.hh output.hh
newsrc.o: newsrc.cc newsrc.hh util.hh
nntpcl.o: nntpcl.cc global.hh areas.hh newsrc.hh nntp.hh nntpcl.hh \
 util.hh
output.o: output.cc global.hh areas.hh newsrc.hh output.hh
ownsoup.o: ownsoup.cc
pop3.o: pop3.cc areas.hh global.hh newsrc.hh output.hh pop3.hh util.hh
qsoup.o: qsoup.cc
reply.o: reply.cc global.hh areas.hh newsrc.hh reply.hh output.hh \
 util.hh nntpcl.hh smtp.hh
smtp.o: smtp.cc global.hh areas.hh newsrc.hh smtp.hh output.hh util.hh
util.o: util.cc util.hh
