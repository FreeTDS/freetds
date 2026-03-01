$ orig_default = f$environment("DEFAULT")
$ last = 1
$
$ set default 'P1'
$ if f$search("recheck.lis") .eqs. ""
$ then 
$!   write sys$output "Nothing to recheck in ''P1'."
$   set default 'orig_default'
$   exit last
$ endif
$ on error then goto Tidy
$ open/read recheck recheck.lis
$
$ loop_files:
$ read/end_of_file=Eof recheck file
$ set noon
$ @[---.vms]full-test 'file'
$ last = $status
$ set on
$ if last .eq. 1552 then goto Tidy  !Ctrl-Y
$ goto loop_files
$!
$ Eof:
$ last = 1
$ Tidy:
$ set default 'orig_default'
$ close recheck
$ exit last
