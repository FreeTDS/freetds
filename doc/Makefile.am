# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.20 2003-09-24 20:28:21 freddy77 Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
RELEASE = 0.61
PRODUCT = FreeTDS

man_MANS	= freebcp.1 tsql.1

EXTRA_DIST	= api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt tds_layer.txt CodingStyle tds.html  \
		userguide.sgml \
		freebcp.1 freebcp.txt tsql.1 tsql.txt  \
		userguide.tgz reference.tgz

html::	userguide.tgz reference.tgz

dist:	userguide.tgz man reference.tgz

# To make the userguide, export DOCBOOK_DSL TO point to docbook.dsl.

userguide.tgz: $(srcdir)/userguide.sgml
	if test -n "${DOCBOOK_DSL}"; then \
	        rm -rf html && \
	        mkdir html && \
	        cd html && pwd && \
	        openjade -d ${DOCBOOK_DSL} -t sgml ../$(srcdir)/userguide.sgml; \
	        test -f book1.htm && \
	        ln -s book1.htm index.html && cd .. && \
	        if ! [ -L userguide ]; then \
	                ln -s html userguide; \
	        fi; \
	        tar czf userguide.tgz userguide/*; \
	fi

man:	freebcp.1 tsql.1

freebcp.1: freebcp.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/freebcp.txt >freebcp.1
	
tsql.1: tsql.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/tsql.txt >tsql.1

reference::
	cd .. && make doxy
	
reference.tgz: reference
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

