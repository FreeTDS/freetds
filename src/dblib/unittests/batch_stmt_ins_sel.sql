create table #batch_stmt(id int)
insert into #batch_stmt values(1)
insert into #batch_stmt values(2)
insert into #batch_stmt values(3)
go
create table #batch_stmt_ins_sel(id int)
insert into #batch_stmt_ins_sel select id from #batch_stmt
select * from #batch_stmt_ins_sel
update #batch_stmt_ins_sel set id = 4
select * from #batch_stmt_ins_sel
drop table #batch_stmt_ins_sel
