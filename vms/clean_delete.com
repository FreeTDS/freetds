$! Delete file without complaining if it doesn't exists
$ if P1 .eqs. "" then exit 1
$ if P1 .eqs. "*;*" then exit 44
$ if P1 .eqs. "*.*;*" then exit 44
$ if P1 .eqs. "[...]*;*" then exit 44
$ if P1 .eqs. "[...]*.*;*" then exit 44
$ if f$search(P1) .nes. "" then delete/noconfirm 'P1'
