-- PARAM:warning 20074
-- Test truncation of character and binary data too long for the server column.
-- Data are truncated (like Sybase library) and SYBEBCOR (20074) is reported.
create table #dblib0016 (id int not null, vc varchar(4) not null, vb varbinary(32) not null, c char(1) not null)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 order by id
go
drop table #dblib0016
go
