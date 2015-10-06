AC_DEFUN([ACX_PUSH_LIBS],
[m4_pushdef([SAVELIBS],[LIBS_]__line__)dnl
SAVELIBS="$LIBS"
LIBS=[$1]])

AC_DEFUN([ACX_POP_LIBS],
[LIBS="$SAVELIBS"
unset SAVELIBS dnl
m4_popdef([SAVELIBS])dnl
])

