# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.24 2003-12-08 07:35:06 jklowden Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
RELEASE = 0.62
PRODUCT = FreeTDS

man_MANS	= freebcp.1 tsql.1

EXTRA_DIST	= api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt tds_layer.txt CodingStyle tds.html  \
		userguide.sgml \
		freebcp.1 freebcp.txt tsql.1 tsql.txt  \
		userguide.tgz reference.tgz

tarballs:	userguide.tgz reference.tgz

dist:	userguide.tgz man reference.tgz

# To make the userguide, export DOCBOOK_DSL to point to docbook.dsl.
ug: userguide.tgz
userguide.tgz: $(srcdir)/userguide.sgml userguide/index.htm
	tar -czf userguide.tgz userguide/*; 
	
userguide/index.htm: $(srcdir)/userguide.sgml userguide
	cd userguide && \
	if test -n "${DOCBOOK_DSL}"; then \
	        openjade -d ../userguide.dsl -t sgml ../$(srcdir)/userguide.sgml; \
	else	\
		echo '<html><P>at <a HREF="http://www.freetds.org/userguide/">www.freetds.org</a></html>' 	\
			> index.htm; \
	fi
	test -f userguide/index.htm
	cd userguide && if [ ! -L index.html ]; then ln -s index.htm index.html; fi

userguide: $(srcdir)/userguide.sgml userguide.dsl
	if [ ! -d userguide ]; then mkdir userguide; fi

userguide.dsl: $(srcdir)/userguide.dsl.in
	sed -ne's!SYSTEM "docbook.dsl" CDATA!SYSTEM "${DOCBOOK_DSL}" CDATA!; p' \
		$(srcdir)/userguide.dsl.in > .userguide.dsl
	mv .userguide.dsl userguide.dsl
	
man:	freebcp.1 tsql.1

freebcp.1: freebcp.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/freebcp.txt >freebcp.1
	
tsql.1: tsql.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/tsql.txt >tsql.1

reference::
	cd .. && make doxy
	
# below is a trick to be sure reference is up-to-date
reference.tgz: ../ChangeLog
	if test ! -f $(srcdir)/reference.tgz; then cd .. && make doxy; fi
	tar czf reference.tgz reference
	
install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(datadir)/doc/freetds
	if test -f $(srcdir)/reference.tgz; then \
		cp $(srcdir)/reference.tgz $(DESTDIR)$(datadir)/doc/freetds/reference.tgz; \
	else \
		cp ./reference.tgz $(DESTDIR)$(datadir)/doc/freetds/reference.tgz; \
	fi
	cd $(DESTDIR)$(datadir)/doc/freetds && tar zxf ./reference.tgz
	if test -f $(srcdir)/userguide.tgz; then \
		cp $(srcdir)/userguide.tgz $(DESTDIR)$(datadir)/doc/freetds/userguide.tgz; \
	else \
		cp ./userguide.tgz $(DESTDIR)$(datadir)/doc/freetds/userguide.tgz; \
	fi
	cd $(DESTDIR)$(datadir)/doc/freetds && tar zxf ./userguide.tgz
	rm -f $(DESTDIR)$(datadir)/doc/freetds/reference.tgz
	rm -f $(DESTDIR)$(datadir)/doc/freetds/userguide.tgz

uninstall-local:
	rm -rf $(DESTDIR)$(datadir)/doc/freetds/reference
	rm -rf $(DESTDIR)$(datadir)/doc/freetds/userguide

