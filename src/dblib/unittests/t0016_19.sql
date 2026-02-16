-- PARAM:hints KEEP_NULLS
-- test keep_nulls hint
-- note: hint should cause error for non-nullable fixed fields, even on Sybase
create table #dblib0016 (id int not null, test1 varchar(20) default 'hello' not null, test2 varchar(20) null, test3 int default 854 null, dt datetime default '2026-02-27 02:34:56.700' null)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
