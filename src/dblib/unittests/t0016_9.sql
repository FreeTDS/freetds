-- check if bit are all collased together, the layout will be
-- 8 bits (b1-b8), i1, 1 bit (b9) -> 6 bytes
create table #dblib0016 (b1 bit, b2 bit, b3 bit, b4 bit, b5 bit, b6 bit, b7 bit, b8 bit, i1 int not null, b9 bit)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
