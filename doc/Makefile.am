# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.29 2003-12-22 10:03:17 freddy77 Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
DOCDIR = doc/freetds-$(VERSION)
PRODUCT = FreeTDS

EXTRA_DIST	= api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt tds_layer.txt CodingStyle tds.html  \
		userguide.sgml \
		freebcp.1 freebcp.txt tsql.1 tsql.txt  \
		userguide.dsl.in userguide.dsl
man_MANS	= freebcp.1 tsql.1

dist:	man

man:	freebcp.1 tsql.1

freebcp.1: freebcp.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) $(srcdir)/freebcp.txt >freebcp.1
	
tsql.1: tsql.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) $(srcdir)/tsql.txt >tsql.1

nobase_dist_data_DATA	= $(DOCDIR)/userguide/* $(DOCDIR)/reference/*

# To make the userguide, export DOCBOOK_DSL to point to docbook.dsl.
$(DOCDIR)/userguide/*: $(DOCDIR)/userguide/index.htm

$(DOCDIR)/userguide/index.htm: $(srcdir)/userguide.sgml userguide.dsl
	test -e doc || mkdir doc
	test -e $(DOCDIR) || mkdir $(DOCDIR)
	test -e $(DOCDIR)/userguide || mkdir $(DOCDIR)/userguide
	rm -f $(DOCDIR)/userguide/*.htm
	cd $(DOCDIR)/userguide && \
	if test -n "${DOCBOOK_DSL}" ; then \
	        openjade -d ../../../userguide.dsl -t sgml ../../../$(srcdir)/userguide.sgml; \
	else	\
		echo '<html><P>at <a HREF="http://www.freetds.org/userguide/">www.freetds.org</a></html>' 	\
			> index.htm; \
	fi
	test -f $(DOCDIR)/userguide/index.htm
	cd $(DOCDIR)/userguide && if test ! -L index.html ; then ln -s index.htm index.html; fi

userguide.dsl: $(srcdir)/userguide.dsl.in
	sed -ne's!SYSTEM "docbook.dsl" CDATA!SYSTEM "${DOCBOOK_DSL}" CDATA!; p' \
		$(srcdir)/userguide.dsl.in > .userguide.dsl
	mv .userguide.dsl userguide.dsl
	
# below is a trick to be sure reference is up-to-date
$(DOCDIR)/reference/*: $(DOCDIR)/reference/index.html

$(DOCDIR)/reference/index.html: $(top_srcdir)/ChangeLog
	cd .. && $(MAKE) doxy

install-data-local: $(DOCDIR)/reference/index.html $(DOCDIR)/userguide/index.htm
	test -e doc || mkdir doc
	test -e $(DOCDIR) || mkdir $(DOCDIR)
	if test ! -e $(DOCDIR)/reference; then cd $(DOCDIR) && ln -s ../../$(srcdir)/$(DOCDIR)/reference reference; fi
	if test ! -e $(DOCDIR)/userguide; then cd $(DOCDIR) && ln -s ../../$(srcdir)/$(DOCDIR)/userguide userguide; fi

uninstall-local:
	rm -rf $(DESTDIR)$(datadir)/$(DOCDIR)/reference
	rm -rf $(DESTDIR)$(datadir)/$(DOCDIR)/userguide

DISTCLEANFILES = $(DOCDIR)/userguide/* $(DOCDIR)/reference/*

