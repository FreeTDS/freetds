# $Id: Makefile.am,v 1.53 2006-06-21 15:05:09 freddy77 Exp $
TESTS		=	\
			t0001$(EXEEXT) t0002$(EXEEXT) t0003$(EXEEXT)\
			t0004$(EXEEXT) connect$(EXEEXT) print$(EXEEXT)\
			date$(EXEEXT) norowset$(EXEEXT) funccall$(EXEEXT)\
			lang_error$(EXEEXT) tables$(EXEEXT)\
			binary_test$(EXEEXT) moreandcount$(EXEEXT)\
			earlybind$(EXEEXT) putdata$(EXEEXT) params$(EXEEXT)\
			raiserror$(EXEEXT) getdata$(EXEEXT)\
			transaction$(EXEEXT) type$(EXEEXT) genparams$(EXEEXT)\
			preperror$(EXEEXT) prepare_results$(EXEEXT)\
			testodbc$(EXEEXT) data$(EXEEXT) error$(EXEEXT)\
			rebindpar$(EXEEXT) convert_error$(EXEEXT)\
			typeinfo$(EXEEXT) const_params$(EXEEXT)\
			insert_speed$(EXEEXT) compute$(EXEEXT)\
			timeout$(EXEEXT) array$(EXEEXT) array_out$(EXEEXT)\
			cursor1$(EXEEXT) scroll$(EXEEXT) cursor2$(EXEEXT)\
			describecol$(EXEEXT) copydesc$(EXEEXT)\
			prepclose$(EXEEXT)

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h
t0003_SOURCES	=	t0003.c common.c common.h
t0004_SOURCES	=	t0004.c common.c common.h
connect_SOURCES	=	connect.c common.c common.h
print_SOURCES	=	print.c common.c common.h
date_SOURCES	=	date.c common.c common.h
norowset_SOURCES	= norowset.c common.c common.h
funccall_SOURCES	= funccall.c common.c common.h
lang_error_SOURCES	= lang_error.c common.c common.h
tables_SOURCES	= tables.c common.c common.h
binary_test_SOURCES	= binary_test.c common.c common.h
moreandcount_SOURCES	= moreandcount.c common.c common.h
earlybind_SOURCES	= earlybind.c common.c common.h
putdata_SOURCES	= putdata.c common.c common.h
params_SOURCES	= params.c common.c common.h
raiserror_SOURCES	= raiserror.c common.c common.h
getdata_SOURCES	= getdata.c common.c common.h
type_SOURCES	= type.c common.c common.h
transaction_SOURCES = transaction.c common.c common.h
genparams_SOURCES = genparams.c common.c common.h
preperror_SOURCES = preperror.c common.c common.h
prepare_results_SOURCES = prepare_results.c common.c common.h
testodbc_SOURCES	= testodbc.c common.c common.h
data_SOURCES	= data.c common.c common.h
error_SOURCES	= error.c common.c common.h
rebindpar_SOURCES	= rebindpar.c common.c common.h
convert_error_SOURCES	= convert_error.c common.c common.h
typeinfo_SOURCES	= typeinfo.c common.c common.h
const_params_SOURCES	= const_params.c common.c common.h
insert_speed_SOURCES	= insert_speed.c common.c common.h
compute_SOURCES	= compute.c common.c common.h
timeout_SOURCES	= timeout.c common.c common.h
array_SOURCES	= array.c common.c common.h
array_out_SOURCES	= array_out.c common.c common.h
cursor1_SOURCES	= cursor1.c common.c common.h
scroll_SOURCES	= scroll.c common.c common.h
cursor2_SOURCES	= cursor2.c common.c common.h
describecol_SOURCES	= describecol.c common.c common.h
copydesc_SOURCES	= copydesc.c common.c common.h
prepclose_SOURCES = prepclose.c common.c common.h

AM_CPPFLAGS	=	-I$(top_srcdir)/include $(ODBC_INC) -DFREETDS_SRCDIR=\"$(srcdir)\"
AM_LDFLAGS	=	-no-install
LIBS		=	$(ODBCLIB) $(ODBCNODMLIB) @NETWORK_LIBS@
DISTCLEANFILES	=	odbc.ini odbcinst.ini

msvc.mak: Makefile
	echo > $@
	echo '!IF "$$(OS)" == "Windows_NT"' >> $@
	echo 'NULL=' >> $@
	echo '!ELSE' >> $@
	echo 'NULL=nul' >> $@
	echo '!ENDIF' >> $@
	echo '' >> $@
	echo 'OUTDIR=.\Release' >> $@
	echo 'INTDIR=.\Release' >> $@
	echo '' >> $@
	list='$(TESTS)'; all='ALL : '; \
	for f in $$list; do \
		all="$$all \\"$$'\n\t'"\"\$$(OUTDIR)\\$$f.exe\""; \
	done; echo "$$all" >> $@
	echo '' >> $@
	echo 'CLEAN :' >> $@
	echo "	-@erase \"\$$(INTDIR)\\*.obj\"" >> $@
	echo "	-@erase \"\$$(OUTDIR)\\*.exe\"" >> $@
	echo '' >> $@
	echo "\"\$$(OUTDIR)\" :" >> $@
	echo "    if not exist \"\$$(OUTDIR)/\$$(NULL)\" mkdir \"\$$(OUTDIR)\"" >> $@
	echo '' >> $@
	echo 'CPP=cl.exe' >> $@
	echo "CPP_PROJ=/nologo /MD /W3 /O2 /Ob2 /I \"./\" /D WIN32 /D NDEBUG /D _CONSOLE /D _MBCS /Fo\"\$$(INTDIR)\\\\\" /Fd\"\$$(INTDIR)\\\\\" /FD /c " >> $@
	echo '' >> $@
	echo '.c{$$(INTDIR)}.obj::' >> $@
	echo "	\$$(CPP) \$$(CPP_PROJ) \$$<" >> $@
	echo '' >> $@
	echo 'LINK32=link.exe' >> $@
	echo 'LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib ws2_32.lib odbc32.lib /nologo /subsystem:console /incremental:no /machine:I386' >> $@
	echo '' >> $@
	list='$(TESTS)'; \
	for f in $$list; do \
		echo "\"\$$(OUTDIR)\\$$f.exe\" : \"\$$(OUTDIR)\" \"\$$(INTDIR)\\common.obj\" \"\$$(INTDIR)\\$$f.obj\"" >> $@; \
		echo "	\$$(LINK32) \$$(LINK32_FLAGS) \"\$$(INTDIR)\\common.obj\" \"\$$(INTDIR)\\$$f.obj\" /pdb:\"\$$(OUTDIR)\\$$f.pdb\" /out:\"\$$(OUTDIR)\\$$f.exe\"" >> $@; \
		echo '' >> $@; \
	done
	echo '' >> $@
	echo 'CHECK :' >> $@
	list='$(TESTS)'; \
	for f in $$list; do \
		echo "	\"\$$(OUTDIR)\\$$f.exe\"" >> $@; \
	done
	echo '' >> $@

