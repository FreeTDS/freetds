# version $Id: Makefile.am,v 1.1 2002-10-09 17:05:44 castellano Exp $

noinst_SCRIPTS	=	unixodbc.freetds.driver.template

EXTRA_DIST	=	README debug.c dyntest.c odbc.ini odbctest.php \
			odbctest.pl test.php test.pl \
			unixodbc.freetds.driver.template.in \
			unixodbc.install.sh \
			unixodbc.jdbc.datasource.template

SUFFIXES	=	.in

edit = sed \
	-e 's,@datadir\@,$(pkgdatadir),g' \
	-e 's,@libdir\@,$(libdir),g' \
	-e 's,@prefix\@,$(prefix),g'

unixodbc.freetds.driver.template: unixodbc.freetds.driver.template.in

.in:
	rm -f $@ $@.tmp
	$(edit) $< >$@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@
