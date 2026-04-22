-- test default values for non-nullable fixed field (Sybase only)
-- Also tests "lock datarows" on temp tables and small rows.
create table #dblib0016 (id int not null, nnint int default 854 not null, nndt datetime default '2026-02-27 02:34:56.700' not null) lock datarows
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
