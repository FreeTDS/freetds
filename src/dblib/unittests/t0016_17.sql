-- test default values for Sybase & MSSQL
-- note: TDS protocol doesn't allow NULL to be encoded for non-nullable
-- fixed fields; however VARCHAR NOT NULL fields can still have NULL sent.
-- Sybase server would reject; MS server applies the default.
-- (Sybase expects the client to implement default values; MS is all serverside)
create table #dblib0016 (id int not null, test1 varchar(20) default 'hello' not null, test2 varchar(20) null, test3 int default 854 null, dt datetime default '2026-02-27 02:34:56.700' null)
go
select * from #dblib0016 where 0=1
go
select * from #dblib0016 where 0=1
go
drop table #dblib0016
go
