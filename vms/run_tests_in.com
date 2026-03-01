$ orig_default = f$environment("DEFAULT")
$ file = f$search("''p1'*.EXE")
$ if file .eqs. ""
$ then
$	write sys$error "no .EXE files in ''p1'"
$	exit %X00018290
$ endif
$
$ last = 1
$ set default 'p1'
$ loop_files:
$ file = f$search("*.EXE")
$ if file .eqs. "" then goto Tidy
$ set noon
$ @[---.vms]full-test 'file'
$ last = $status
$ set on
$ if last .eq. 1552 then goto Tidy  !Ctrl-Y
$ goto loop_files
$!
$ Tidy:
$ set default 'orig_default'
$ exit last
