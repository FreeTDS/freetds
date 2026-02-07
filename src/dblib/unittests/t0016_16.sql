-- test pretty long string overflowing 256 bytes in row data for Sybase
create table #dblib0016 (id int not null, test1 varchar(200) null, test2 varchar(200) null, test3 varchar(200) null)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
