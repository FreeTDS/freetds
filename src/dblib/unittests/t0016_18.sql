-- PARAM:table dblib0016view
-- PARAM:hints fire_triggers

-- test triggers for Sybase
if object_id('dblib0016trigger') is not null drop trigger dblib0016trigger
if object_id('dblib0016view') is not null drop view dblib0016view
if object_id('dblib0016table') is not null drop table dblib0016table

exec('create table dblib0016table(id int, vc varchar(100) not null)')
exec('create view dblib0016view as select id, concat(''pre'', vc) as vc2 from dblib0016table')
exec('create trigger dblib0016trigger on dblib0016view instead of insert as
begin insert into dblib0016table select id, concat(vc2, ''post'') from inserted end')
go
select * from dblib0016view where 0=1
go
select * from dblib0016view where 0=1
go
drop trigger dblib0016trigger
drop view dblib0016view
drop table dblib0016table
go
