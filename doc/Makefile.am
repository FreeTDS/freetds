# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.19 2003-08-05 05:03:15 jklowden Exp $
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
	        tar czf userguide.tgz userguide/* \
	; fi

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
	ln -s `pwd`/reference.tgz $(DESTDIR)$(datadir)/doc/freetds \
		|| touch $(DESTDIR)$(datadir)/doc/freetds/reference.tgz
	ln -s `pwd`/userguide.tgz $(DESTDIR)$(datadir)/doc/freetds \
		|| touch $(DESTDIR)$(datadir)/doc/freetds/userguide.tgz
	cd $(DESTDIR)$(datadir)/doc/freetds || pwd # 'cd' can return spurious errors?
	tar xzf reference.tgz || echo No reference manual installed
	tar xzf userguide.tgz || echo No user guide installed
	rm reference.tgz userguide.tgz
