
# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140

# $Id: Makefile.am,v 1.66 2011-04-09 12:49:23 jklowden Exp $

SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
DOCDIR = doc/freetds-$(VERSION)
PRODUCT = FreeTDS
TARGET_DOCDIR = $(DESTDIR)$(datadir)/$(DOCDIR)

IMAGES = 	images/important.gif \
		images/note.gif \
		images/tip.gif \
		images/caution.gif \
		images/warning.gif

CALLOUTS =	images/callouts/1.gif \
		images/callouts/2.gif \
		images/callouts/3.gif \
		images/callouts/4.gif \
		images/callouts/5.gif \
		images/callouts/6.gif \
		images/callouts/7.gif \
		images/callouts/8.gif \
		images/callouts/9.gif \
		images/callouts/10.gif


EXTRA_DIST =	api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt CodingStyle tds.html  \
		userguide.dsl.in userguide.sgml userguide.css \
		$(man_MANS) freebcp.txt tsql.txt osql.txt fisql.txt \
		bsqldb.txt bsqlodbc.txt defncopy.txt datacopy.txt \
		$(DOCDIR)/userguide $(DOCDIR)/reference \
		images grep_sample_code \
		tds_ssl.html

dist-hook:
	find $(distdir)/doc -name CVS -exec rm -rf {} ';'

man_MANS	= freebcp.1 tsql.1 osql.1 bsqldb.1 bsqlodbc.1 defncopy.1 datacopy.1 fisql.1 \
		  freetds.conf.5
 
noinst_DATA	= $(DOCDIR)/reference/index.html $(DOCDIR)/userguide/index.htm

$(DOCDIR)/userguide: $(DOCDIR)/userguide/index.htm

$(DOCDIR)/reference: $(DOCDIR)/reference/index.html

doc:  $(DOCDIR)/userguide $(DOCDIR)/reference

dist:	man

man:	$(man_MANS)

installdirs: 
	$(mkinstalldirs)	$(TARGET_DOCDIR)/userguide    \
				$(TARGET_DOCDIR)/reference    \
				$(TARGET_DOCDIR)/images       \
				$(TARGET_DOCDIR)/images/callouts 2>&1

MANOPTS = -I 'FreeTDS User Guide'  -v 'FreeTDS Utilities' -t $(PRODUCT) -r $(VERSION)

freebcp.1: freebcp.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/freebcp.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

tsql.1: tsql.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/tsql.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

osql.1: osql.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/osql.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

fisql.1: fisql.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/fisql.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

bsqldb.1: bsqldb.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/bsqldb.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

bsqlodbc.1: bsqlodbc.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/bsqlodbc.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

defncopy.1: defncopy.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/defncopy.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

datacopy.1: datacopy.txt
	$(TXT2MAN) $(MANOPTS) $(srcdir)/datacopy.txt >.$@
	if test ! -s .$@; then echo 'unable to generate $@'; fi
	test -s .$@
	mv .$@ $@

# If we built the documentation ourselves, install that, 
# else if we have access to distributed documentation, install that.  
install-data-local: installdirs $(noinst_DATA)
	$(mkinstalldirs) $(DOCDIR) 2>&1
	if test -r $(DOCDIR)/userguide ; then d=.; else d="$(srcdir)"; fi; \
	find $$d/$(DOCDIR)/userguide \( -type f -o -type l \) -exec \
		$(INSTALL_DATA) {} $(TARGET_DOCDIR)/userguide ';'
	if test -r $(DOCDIR)/reference ; then d=.; else d="$(srcdir)"; fi; \
	find $$d/$(DOCDIR)/reference \( -type f -o -type l \) -exec \
		$(INSTALL_DATA) {} $(TARGET_DOCDIR)/reference ';'
	cd $(srcdir) && find images -name \*.gif -exec \
		$(INSTALL_DATA) {} $(TARGET_DOCDIR)/{} ';'

uninstall-local:
	rm -rf $(TARGET_DOCDIR)

clean-local:
	rm -rf userguide.dsl doxy.log userguide reference \
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
	$(mkinstalldirs) $(DOCDIR)/userguide 2>&1
	touch $(DOCDIR)/userguide/t.htm
	rm -f $(DOCDIR)/userguide/*
	$(MAKE) dblib.api.sgml ctlib.api.sgml odbc.api.sgml
# if we're not using jade, copy the distribution's userguide
	if test -z "${DOCBOOK_DSL}" ; then \
		if test -r $(DISTRIBUTED_UG_DIR) -a ! -r $(DOCDIR)/userguide ; then \
			cp -R $(DISTRIBUTED_UG_DIR) $(DOCDIR); \
		fi; \
	fi
# try jade ...
	cd $(DOCDIR)/userguide && \
	if test -n "${DOCBOOK_DSL}" ; then \
##		The crazy path is due to the above 'cd' command
		SRCDOC=../../../$(srcdir); \
		if test ! -r userguide.sgml; then ln -s $${SRCDOC}/userguide.sgml userguide.sgml; fi; \
		openjade -b utf-8 -w all -w no-empty -w no-unused-param \
			 -d ../../../userguide.dsl -t sgml userguide.sgml; \
		if test -h userguide.sgml; then rm userguide.sgml; fi; \
## 		and copy the stylesheet
		cp $${SRCDOC}/userguide.css .; \
	fi
# if still no joy, create a stub
	if test ! -r $@ ; then \
		echo '<html><P>at <a HREF="http://www.freetds.org/userguide/">www.freetds.org</a></html>' 	\
		> .index.htm && \
		mv .index.htm $@; \
	fi
	test -f $(DOCDIR)/userguide/index.htm
	cd $(DOCDIR)/userguide && if test ! -h index.html ; then ln -s index.htm index.html; fi
# for convenience, create a link in the build directory
	if test -h userguide; then rm userguide; fi
	ln -sf $(DOCDIR)/userguide userguide

userguide: $(DOCDIR)/userguide/index.htm
reference: $(DOCDIR)/reference/index.html

DISTRIBUTED_REF_DIR = $(srcdir)/$(DOCDIR)/reference

$(DOCDIR)/reference/index.html: $(top_srcdir)/ChangeLog
	$(mkinstalldirs) $(DOCDIR) 2>&1
	cd .. && $(MAKE) doxy 2> doc/doxy.log
# if "make doxy" doesn't create anything, try to copy the distributed manual's HTML
	if test -r $(DISTRIBUTED_REF_DIR)/index.html -a ! $@ ; then \
		cp -R $(DISTRIBUTED_REF_DIR) $(DOCDIR); \
	fi
# if we don't find the source, generate a stub
	$(mkinstalldirs) $(DOCDIR)/reference 2>&1
	if test ! -r $@ ; then \
		cd $(DOCDIR)/reference && \
		echo '<html><P>at <a HREF="http://www.freetds.org/reference/">www.freetds.org</a></html>' 	\
			> .index.html &&  \
		mv .index.html index.html; \
	fi
# for convenience, create a link in the build directory
	if test -h reference; then rm reference; fi
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
dblib.api.sgml:
	echo '<table id="dblib.api"><title>db-lib API Implementation Summary</title>' > .dblib.api
	echo '<tgroup cols=5><thead><row>' \
  		'<entry>Category</>' \
  		'<entry>Microsoft name</>' \
  		'<entry>Sybase name</>' \
  		'<entry>Status</>' \
  		'<entry>Comments</>' \
		'</row></thead>' \
		'<tbody>' \
		>> .dblib.api
	grep '^dblib' $(srcdir)/api_status.txt | \
		sed -e 's!^dblib[	]!<row><entry>!; s![	][	]*!</> <entry>!g; s!$$!</></row>!; ' \
		>> .dblib.api
	echo '</tbody></tgroup></table>' >> .dblib.api
	mv .dblib.api $@

ctlib.api.sgml:
	echo '<table id="ctlib.api" colsep=0>' \
		'<title>ct-lib API Implementation Summary</title>' \
		> .ctlib.api
	echo '<tgroup cols=4><thead><row>' \
		'<entry>Category</>' \
		'<entry>Function</>' \
  		'<entry>Status</>' \
  		'<entry>Comments</>' \
		'</row></thead>' \
		'<tbody>' \
			>> .ctlib.api
	grep '^ctlib' $(srcdir)/api_status.txt | \
		sed -e 's!^ctlib[	]!<row><entry>!; s![	]!</> <entry>!g; s!$$!</></row>!; ' \
		>> .ctlib.api
	echo '</tbody></tgroup></table>' \
		>> .ctlib.api
	mv .ctlib.api $@

MSDN_ODBC = http://msdn.microsoft.com/library/en-us/odbc/htm/odbc

odbc.api.sgml:
	echo '<table id="odbc.api"><title>ODBC API Implementation Summary</title>' \
			> .odbc.api
	echo '<tgroup cols=4><thead><row>' \
		'<entry>Category</>' \
		'<entry>Function</>' \
		'<entry>Status</>' \
		'<entry>Comments</>' \
		'</row></thead>' \
		'<tbody>' \
			>> .odbc.api
# The mean RE below (with "ulink" in it) forms a URL to Microsoft's reference page
# for the function.  It assumes a word starting with "SQL" is an ODBC function. 
# Let's hope they don't change their site too often.  
	grep '^odbc' $(srcdir)/api_status.txt | \
		sed -e' s!^odbc[	]!<row><entry>!; s![	][	]*!</> <entry>!g; s!\(SQL[[:alpha:]]*\)!<ulink url="$(MSDN_ODBC)\1.asp">\1</ulink>!g; s!$$!</></row>!; ' \
		>> .odbc.api
	echo '</tbody></tgroup></table>' \
		>> .odbc.api
	mv .odbc.api $@



DISTCLEANFILES = $(DOCDIR)/userguide $(DOCDIR)/reference

