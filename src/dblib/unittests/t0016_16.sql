create table #dblib0016 (id int not null, nndt datetime DEFAULT('2026-01-26 01:23:45.677') NOT NULL, str VARCHAR(20) DEFAULT('The quick brown fox') NULL, dt datetime DEFAULT('2026-02-27 02:34:56.700') NULL) lock datarows
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
