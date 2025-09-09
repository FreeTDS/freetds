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
$ DEFINE/NOLOG/USER_MODE SYS$ERROR "''file'_error.log"
$ IF tds_full_test_dump .GT. 0 THEN DEFINE/USER TDSDUMP "''file'_dump.log"
$ starttime = F$TIME()
$ RUN/NODEBUG 'P1'
$ endtime = F$TIME()
$ deltatime = F$DELTA_TIME(starttime, endtime, "dcl")
$ deltasecs = F$CVTIME(deltatime,"delta","minute") * 60 + F$CVTIME(deltatime,"delta","second")
$ timinginfo = " "
$ if deltasecs .gt. 5 then timinginfo = "(took ''deltasecs' seconds)"
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
$        WRITE SYS$OUTPUT F$FAO("ok !20AS  !AS", file, timinginfo)
$    ENDIF
$    IF F$TYPE(olddef) .NES. "" THEN SET DEFAULT &olddef
$    IF F$TYPE(oldmsg) .NES. "" THEN SET MESSAGE 'oldmsg'
$    IF F$TYPE(oldpriv) .NES. "" THEN discard = F$SETPRV(oldpriv)
$    EXIT status
