-- test default values for Sybase
create table #dblib0016 (id int not null, test1 varchar(20) default 'hello', test2 varchar(20) null, test3 unsigned int default 854)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
