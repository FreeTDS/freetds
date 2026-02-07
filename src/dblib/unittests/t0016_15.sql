-- test NULL TEXT values, it was broken for Sybase
create table #dblib0016 (id int not null, test1 text null, test2 text null)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
