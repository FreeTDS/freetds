# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.41 2003-12-31 08:04:52 freddy77 Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
DOCDIR = doc/freetds-$(VERSION)
PRODUCT = FreeTDS
TARGET_DOCDIR = $(DESTDIR)$(datadir)/$(DOCDIR)

EXTRA_DIST	= api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt tds_layer.txt CodingStyle tds.html  \
		userguide.dsl.in userguide.sgml \
		freebcp.1 freebcp.txt tsql.1 tsql.txt  \
		$(DOCDIR)/userguide $(DOCDIR)/reference \
		images/important.gif images/note.gif images/tip.gif

man_MANS	= freebcp.1 tsql.1
nobase_data_DATA	= $(DOCDIR)/reference/index.html $(DOCDIR)/userguide/index.htm

$(DOCDIR)/userguide: $(DOCDIR)/userguide/index.htm

$(DOCDIR)/reference: $(DOCDIR)/reference/index.html

doc:  $(DOCDIR)/userguide $(DOCDIR)/reference

dist:	man

man:	freebcp.1 tsql.1

installdirs: 
	$(mkinstalldirs)	$(TARGET_DOCDIR)/userguide    \
				$(TARGET_DOCDIR)/reference    \
				$(TARGET_DOCDIR)/images

freebcp.1: freebcp.txt
	$(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) $(srcdir)/freebcp.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

tsql.1: tsql.txt
	$(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) $(srcdir)/tsql.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

# If we built the documentation ourselves, install that, 
# else if we have access to distributed documentation, install that.  
install-data-local: installdirs $(DOCDIR)/reference/index.html $(DOCDIR)/userguide/index.htm
	$(mkinstalldirs) $(DOCDIR)
	if test -r $(DOCDIR)/userguide ; then d=.; else d="$(srcdir)"; fi; \
	find $$d/$(DOCDIR)/userguide \( -type f -o -type l \) -exec \
		$(INSTALL_DATA) {} $(TARGET_DOCDIR)/userguide ';'
	if test -r $(DOCDIR)/reference ; then d=.; else d="$(srcdir)"; fi; \
	find $$d/$(DOCDIR)/reference \( -type f -o -type l \) -exec \
		$(INSTALL_DATA) {} $(TARGET_DOCDIR)/reference ';'
	find $(srcdir)/images -name \*.gif -exec \
		$(INSTALL_DATA) {} $(TARGET_DOCDIR)/images ';'

uninstall-local:
	rm -rf $(TARGET_DOCDIR)

clean-local:
	rm -rf userguide.dsl doc doxy.log userguide reference \
		dblib.api	\
		ctlib.api	\
		 odbc.api	\
		dblib.api.sgml	\
		ctlib.api.sgml	\
		 odbc.api.sgml   

## In building the UG and reference manual, we confront several possible situations:
## 1.  CVS user (or developer) with or without jade/doxygen installed.
## 2.  Distribution user.  The distribution includes pre-built documentation, ready to install.
## Either of these users may choose to build out-of-tree (e.g., in a build/ directory).  
## We want to build the docs if we can, link to distributed ones if they exist, else create 
## stub files pointing to the website.  
##
## In the case of jade, we build the documentation only if the environment variable DOCBOOK_DSL 
## is set.  A user would define DOCBOOK_DSL only if he intends to build the UG.  If the user
## builds FreeTDS out of tree, "make install" will put a symlink to the distributed UG HTML
## in the build directory, and install per usual.  
##
## In the case of Doxygen, we detect its presence in the configure script, and invoke it if 
## found.  If it is not found, we try to link the build tree's reference manual HTML to the
## distribution.  If that fails, we install a stub.  

DISTRIBUTED_UG_DIR = $(srcdir)/$(DOCDIR)/userguide

# To make the userguide, export DOCBOOK_DSL to point to docbook.dsl.
$(DOCDIR)/userguide/index.htm: userguide.sgml  userguide.dsl.in api_status.txt
	sed -ne's!SYSTEM "docbook.dsl" CDATA!SYSTEM "${DOCBOOK_DSL}" CDATA!; p' \
		$(srcdir)/userguide.dsl.in > .userguide.dsl
	mv .userguide.dsl userguide.dsl
	$(mkinstalldirs) $(DOCDIR)/userguide
	touch $(DOCDIR)/userguide/t.htm
	rm -f $(DOCDIR)/userguide/*
	make dblib.api.sgml
	make ctlib.api.sgml
	make odbc.api.sgml
# if we're not using jade, copy the distribution's userguide
	if test -z "${DOCBOOK_DSL}" ; then \
		if test -r $(DISTRIBUTED_UG_DIR) -a ! -r $(DOCDIR)/userguide ; then \
			cp -R $(DISTRIBUTED_UG_DIR) $(DOCDIR); \
		fi; \
	fi
# try jade ...
	cd $(DOCDIR)/userguide && \
	if test -n "${DOCBOOK_DSL}" ; then \
		if test ! -r userguide.sgml; then ln -s ../../../$(srcdir)/userguide.sgml userguide.sgml; fi; \
		openjade -d ../../../userguide.dsl -t sgml userguide.sgml; \
		if test -L userguide.sgml; then rm userguide.sgml; fi; \
	fi
# if still no joy, create a stub
	if test ! -r $@ ; then \
		echo '<html><P>at <a HREF="http://www.freetds.org/userguide/">www.freetds.org</a></html>' 	\
		> .index.htm && \
		mv .index.htm $@; \
	fi
	test -f $(DOCDIR)/userguide/index.htm
	cd $(DOCDIR)/userguide && if test ! -L index.html ; then ln -s index.htm index.html; fi
# for convenience, create a link in the build directory
	if test -L userguide; then rm userguide; fi
	ln -sf $(DOCDIR)/userguide userguide

userguide: $(DOCDIR)/userguide/index.htm
reference: $(DOCDIR)/reference/index.html

DISTRIBUTED_REF_DIR = $(srcdir)/$(DOCDIR)/reference

$(DOCDIR)/reference/index.html: $(top_srcdir)/ChangeLog
	$(mkinstalldirs) $(DOCDIR)
	cd .. && make doxy 2> doc/doxy.log
# if "make doxy" doesn't create anything, try to copy the distributed manual's HTML
	if test -r $(DISTRIBUTED_REF_DIR)/index.html -a ! $@ ; then \
		cp -R $(DISTRIBUTED_REF_DIR) $(DOCDIR); \
	fi
# if we don't find the source, generate a stub
	$(mkinstalldirs) $(DOCDIR)/reference
	if test ! -r $@ ; then \
		cd $(DOCDIR)/reference && \
		echo '<html><P>at <a HREF="http://www.freetds.org/reference/">www.freetds.org</a></html>' 	\
			> .index.html &&  \
		mv .index.html index.html; \
	fi
# for convenience, create a link in the build directory
	if test -L reference; then rm reference; fi
	ln -sf $(DOCDIR)/reference reference

#
# The following sections generate API implementation reference tables as SGML "entities".
# The entities are used a little bit like include files in the UG, e.g.:  
#	<!ENTITY dblibapisgml SYSTEM "../../../dblib.api.sgml">
#
# and "dblibapisgml" is referred to later on:
#	
#	<sect1 id="dblib.api.summary">
#		<title>db-lib API Implementation Summary</title>
#		&dblibapisgml;
#	</sect1>
#
# This lets us use the api_status.txt document in our SGML.
#
dblib.api.sgml: dblib.api 
	echo '<table id="dblib.api"><title>db-lib API Implementation Summary</title>' > .dblib.api.head
	echo '<tgroup cols=5><thead><row> \
  		<entry>Category</> \
  		<entry>Microsoft name</> \
  		<entry>Sybase name</> \
  		<entry>Status</> \
  		<entry>Comments</> \
		</row></thead> \
		<tbody>' \
			> .tableheader
	sed -e' s!^dblib[	]!<row><entry>!; \
		s![	][	]*!</> <entry>!g; \
		s!$$!</></row>!; \
	      ' dblib.api \
			> .dblib.api.body
	echo '</tbody></tgroup></table>' > .dblib.api.tail
	cat .dblib.api.head .tableheader .dblib.api.body .dblib.api.tail >$@
	rm  .dblib.api.head .tableheader .dblib.api.body .dblib.api.tail

dblib.api: api_status.txt
	grep ^dblib $(srcdir)/api_status.txt > .dblib.api
	test -s  .dblib.api
	mv .dblib.api dblib.api

ctlib.api.sgml: ctlib.api 
	echo '<table id="ctlib.api" colsep=0> \
		<title>ct-lib API Implementation Summary</title>' \
			> .ctlib.api.head
	echo '<tgroup cols=4><thead><row> \
  		<entry>Category</> \
  		<entry>Function</> \
  		<entry>Status</> \
  		<entry>Comments</> \
		</row></thead> \
		<tbody>' \
			> .tableheader
	sed -e' s!^ctlib[	]!<row><entry>!; \
		s![	]!</> <entry>!g; \
		s!$$!</></row>!; \
	      ' ctlib.api \
			> .ctlib.api.body
	echo '</tbody></tgroup></table>' \
			> .ctlib.api.tail
	cat .ctlib.api.head .tableheader .ctlib.api.body .ctlib.api.tail >$@
	rm  .ctlib.api.head .tableheader .ctlib.api.body .ctlib.api.tail

ctlib.api: api_status.txt
	grep ^ctlib $(srcdir)/api_status.txt > .ctlib.api
	test -s  .ctlib.api
	mv .ctlib.api ctlib.api

odbc.api.sgml: odbc.api 
	echo '<table id="odbc.api"><title>ODBC API Implementation Summary</title>' \
			> .odbc.api.head
	echo '<tgroup cols=4><thead><row> \
  		<entry>Category</> \
  		<entry>Function</> \
  		<entry>Status</> \
  		<entry>Comments</> \
		</row></thead> \
		<tbody>' \
			> .tableheader
	sed -e' s!^odbc[	]!<row><entry>!; \
		s![	][	]*!</> <entry>!g; \
		s!$$!</></row>!; \
	      ' odbc.api \
			> .odbc.api.body
	echo '</tbody></tgroup></table>' \
			> .odbc.api.tail
	cat .odbc.api.head .tableheader .odbc.api.body .odbc.api.tail >$@
	rm  .odbc.api.head .tableheader .odbc.api.body .odbc.api.tail

odbc.api: api_status.txt
	grep ^odbc $(srcdir)/api_status.txt > .odbc.api
	test -s  .odbc.api
	mv .odbc.api odbc.api



DISTCLEANFILES = $(DOCDIR)/userguide $(DOCDIR)/reference

