# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.34 2003-12-24 11:22:03 jklowden Exp $
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
		userguide.dsl

man_MANS	= freebcp.1 tsql.1
nobase_data_DATA	= $(DOCDIR)/reference/index.html $(DOCDIR)/userguide/index.htm

$(DOCDIR)/userguide: $(DOCDIR)/userguide/index.htm

$(DOCDIR)/reference: $(DOCDIR)/reference/index.html

doc:  $(DOCDIR)/userguide $(DOCDIR)/reference

dist:	man

man:	freebcp.1 tsql.1

installdirs: 
	$(mkinstalldirs)	$(TARGET_DOCDIR)/userguide    \
				$(TARGET_DOCDIR)/reference

freebcp.1: freebcp.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) $(srcdir)/freebcp.txt >freebcp.1

tsql.1: tsql.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(VERSION) $(srcdir)/tsql.txt >tsql.1

# If we built the documentation ourselves, install that, 
# else if we have access to distributed documentation, install that.  
install-data-local: installdirs $(DOCDIR)/reference/index.html $(DOCDIR)/userguide/index.htm
	$(mkinstalldirs) $(DOCDIR)
	if test -e $(DOCDIR)/userguide ; then d=.; else d="$(srcdir)"; fi; \
	$(INSTALL_DATA) -c $$d/$(DOCDIR)/userguide/* $(TARGET_DOCDIR)/userguide
	if test -e $(DOCDIR)/reference ; then d=.; else d="$(srcdir)"; fi; \
	$(INSTALL_DATA) -c $$d/$(DOCDIR)/reference/* $(TARGET_DOCDIR)/reference

uninstall-local:
	rm -rf $(TARGET_DOCDIR)

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
$(DOCDIR)/userguide/index.htm: $(srcdir)/userguide.sgml userguide.dsl 
	$(mkinstalldirs) $(DOCDIR)/userguide
	touch $(DOCDIR)/userguide/t.htm
	rm    $(DOCDIR)/userguide/*.htm
# if we're not using jade, try linking to the distribution's userguide
	if test -z "${DOCBOOK_DSL}" ; then \
		if test -e $(DISTRIBUTED_UG_DIR) -a ! -e $(DOCDIR)/userguide ; then \
			cp -R $(DISTRIBUTED_UG_DIR) $(DOCDIR); \
		fi; \
	fi
# try jade ...
	cd $(DOCDIR)/userguide && \
	if test -n "${DOCBOOK_DSL}" ; then \
	        openjade -d ../../../userguide.dsl -t sgml ../../../$(srcdir)/userguide.sgml; \
	fi
# if still no joy, create a stub
	if test ! -e $@ ; then \
		echo '<html><P>at <a HREF="http://www.freetds.org/userguide/">www.freetds.org</a></html>' 	\
		> .index.htm; \
		mv .index.html $@; \
	fi
	test -f $(DOCDIR)/userguide/index.htm
	cd $(DOCDIR)/userguide && if test ! -L index.html ; then ln -s index.htm index.html; fi

userguide.dsl: $(srcdir)/userguide.dsl.in
	sed -ne's!SYSTEM "docbook.dsl" CDATA!SYSTEM "${DOCBOOK_DSL}" CDATA!; p' \
		$(srcdir)/userguide.dsl.in > .userguide.dsl
	mv .userguide.dsl $@

reference: $(DOCDIR)/reference/index.html

DISTRIBUTED_REF_DIR = $(srcdir)/$(DOCDIR)/reference

$(DOCDIR)/reference/index.html: $(top_srcdir)/ChangeLog
	cd .. && make doxy
# if "make doxy" doesn't create anything, try linking to the distributed manual's HTML
	if test -e $(DISTRIBUTED_REF_DIR)/index.html -a ! $@ ; then \
		cp -R $(DISTRIBUTED_REF_DIR) $(DOCDIR); \
	fi
# if we don't find the source, generate a stub
	if test ! -e $@ ; then \
		cd $(DOCDIR)/reference && \
		echo '<html><P>at <a HREF="http://www.freetds.org/reference/">www.freetds.org</a></html>' 	\
			> .index.html &&  \
		mv .index.html index.html; \
	fi

DISTCLEANFILES = $(DOCDIR)/userguide $(DOCDIR)/reference

