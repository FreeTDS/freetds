# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140

SHELL = /bin/sh
TXT2MAN = ./txt2man
RELEASE = 0.60
PRODUCT = FreeTDS

man_MANS	= freebcp.1 tsql.1

html:: 
	if ! [ -d html ]; then \
            mkdir html; \
	fi; \
	cd html; pwd; \
	openjade -d ${DOCBOOK}/dsssl/modular/html/docbook.dsl -t sgml ../userguide.sgml; \
	if ! [ -e html/index.html ]; then \
		ln -s book1.htm index.html; \
	fi; 

#	openjade -d ${DOCBOOK}/dsssl/modular/html/docbook.dsl -t sgml ../userguide.sgml

dist::	man html
	if ! [ -L userguide ]; then \
		ln -s html userguide; \
	fi; \
	tar zcf userguide.tgz userguide/* 

man:	freebcp.1 tsql.1

freebcp.1:  freebcp.txt
	$(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) freebcp.txt >freebcp.1
	
tsql.1: tsql.txt
	$(TXT2MAN) -P $(PRODUCT) -t $(PRODUCT) -r $(RELEASE) tsql.txt >tsql.1

	
