# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140
# version: $Id: Makefile.am,v 1.14 2003-01-29 05:09:43 jklowden Exp $
SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
RELEASE = 0.61
PRODUCT = FreeTDS

man_MANS	= freebcp.1 tsql.1

EXTRA_DIST	= api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt tds_layer.txt userguide.sgml \
		basic_jdbc_question.html faq.html tds.html userguide.tgz \
		freebcp.1 freebcp.txt tsql.1 tsql.txt CodingStyle

html::	userguide.tgz

dist:	userguide.tgz man 

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
	        tar zcf userguide.tgz userguide/* \
	; fi

man:	freebcp.1 tsql.1

freebcp.1: freebcp.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/freebcp.txt >freebcp.1
	
tsql.1: tsql.txt
	- $(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/tsql.txt >tsql.1


