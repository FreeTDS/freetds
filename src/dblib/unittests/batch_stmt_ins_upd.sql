create table #batch_stmt_results(id int);
insert into #batch_stmt_results values(1), (2), (3);
update #batch_stmt_results set id = 1;
insert into #batch_stmt_results values(2);
drop table #batch_stmt_results;
