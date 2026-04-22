-- PARAM:colin 1
-- Test omitted columns from bcp (MSSQL does not support)
create table #dblib0016 (i1 unsigned int null, i2 int default(7) null, i3 int default(9) not null)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
