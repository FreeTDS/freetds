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
$ if f$search("recheck.lis") .nes. "" THEN delete/noconfirm recheck.lis;*
$ create recheck.lis
$
$ loop_files:
$ file = f$search("*.EXE")
$ if file .eqs. "" then goto Tidy
$ set noon
$ @[---.vms]full-test 'file'
$ last = $status
$ set on
$ if last .eq. 1552 then goto Tidy  !Ctrl-Y
$ if .not. last
$ then
$   open/append recheck recheck.lis
$!  Strip version; the rebuilt test exe might have a different version
$   file1 = f$element(0,";",file)
$   write recheck "''file1'"
$   close recheck
$ endif
$ goto loop_files
$!
$ Tidy:
$ set default 'orig_default'
$ exit last
