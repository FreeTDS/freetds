$ ON CONTROL_Y THEN GOTO CONTROL_Y_EXIT
$ ON ERROR THEN GOTO WRAPUP
$ olddef = F$ENVIRONMENT("DEFAULT")
$ oldmsg = F$ENVIRONMENT("MESSAGE")
$ oldpriv = F$SETPRV("NOALL")         ! downgrade privs for safety
$ discard = F$SETPRV("NETMBX,TMPMBX") ! only need these to run tests
$ file = F$PARSE("''P1'",,,"NAME", "SYNTAX_ONLY")
$!
$ SET MESSAGE /NOFACILITY/NOSEVERITY/NOIDENTIFICATION/NOTEXT
$ DEFINE/NOLOG/USER_MODE SYS$OUTPUT "''file'.log"
$ DEFINE/NOLOG/USER_MODE SYS$ERROR "''file'.log"
$ RUN/NODEBUG 'P1'
$ GOTO WRAPUP
$!
$ CONTROL_Y_EXIT:
$   $STATUS = 1552   ! %SYSTEM-W-CONTROLY
$!
$ WRAPUP:
$    status = $STATUS
$    IF .NOT. status
$    THEN
$        WRITE SYS$OUTPUT "NOT ok ''file' -- check ''file'.log"
$    ELSE
$        WRITE SYS$OUTPUT "ok ''file'"
$    ENDIF
$    IF F$TYPE(olddef) .NES. "" THEN SET DEFAULT &olddef
$    IF F$TYPE(oldmsg) .NES. "" THEN SET MESSAGE 'oldmsg'
$    IF F$TYPE(oldpriv) .NES. "" THEN discard = F$SETPRV(oldpriv)
$    EXIT status
