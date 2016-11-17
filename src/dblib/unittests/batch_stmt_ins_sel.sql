create table #batch_stmt_ins_sel(id int);
insert into #batch_stmt_ins_sel values(1), (2), (3);
select * from #batch_stmt_ins_sel;
update #batch_stmt_ins_sel set id = 4;
select * from #batch_stmt_ins_sel;
drop table #batch_stmt_ins_sel;
