# Converting DocBook to HTML (several small files)
# http://www.freebsd.org/tutorials/docproj-primer/x3132.html#AEN3140

SHELL = /bin/sh

html::
	if ! [ -d html ]; then \
            mkdir html; \
	fi; \
	cd html; pwd; \
	openjade -d ${DOCBOOK}/dsssl/modular/html/docbook.dsl -t sgml ../userguide.sgml

#	openjade -d ${DOCBOOK}/dsssl/modular/html/docbook.dsl -t sgml ../userguide.sgml
