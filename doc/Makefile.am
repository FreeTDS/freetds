# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140

SHELL = /bin/sh
TXT2MAN = $(srcdir)/txt2man
RELEASE = 0.60
PRODUCT = FreeTDS
#OCBOOK_DSL = /usr/share/sgml/docbook/dsssl-stylesheets/html/docbook.dsl
#OCBOOK_DSL = ${DOCBOOK}/dsssl/modular/html/docbook.dsl
DOCBOOK_DSL = $DOCBOOK_DSL 
man_MANS	= freebcp.1 tsql.1

EXTRA_DIST	= api_status.txt bcp.txt cap.txt getting_started.txt \
		policy.txt tds7.txt tds_layer.txt userguide.sgml \
		basic_jdbc_question.html faq.html tds.html userguide.tgz \
		freebcp.1 tsql.1 CodingStyle

html:: userguide.tgz

#	openjade -d ${DOCBOOK}/dsssl/modular/html/docbook.dsl -t sgml ../userguide.sgml

dist::	userguide.tgz

userguide.tgz: $(srcdir)/userguide.sgml
	rm -rf html
	mkdir html
	cd html && pwd && \
	openjade -d ${DOCBOOK_DSL} -t sgml ../$(srcdir)/userguide.sgml
	test -f html/book1.htm
	cd html && ln -s book1.htm index.html
	if ! [ -L userguide ]; then \
		ln -s html userguide; \
	fi
	tar zcf userguide.tgz userguide/* 

man:	freebcp.1 tsql.1

freebcp.1: freebcp.txt
	$(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/freebcp.txt >freebcp.1
	
tsql.1: tsql.txt
	$(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) $(srcdir)/tsql.txt >tsql.1


