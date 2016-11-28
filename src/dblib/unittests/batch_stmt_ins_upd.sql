create table #batch_stmt (id int)
insert into #batch_stmt values(1)
insert into #batch_stmt values(2)
insert into #batch_stmt values(3)
go
create table #batch_stmt_results(id int)
insert into #batch_stmt_results select id from #batch_stmt
update #batch_stmt_results set id = 1
insert into #batch_stmt_results values(2)
drop table #batch_stmt_results
