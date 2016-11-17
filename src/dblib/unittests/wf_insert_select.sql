create table #wf_insert_select(id int);
insert into #wf_insert_select values(1), (2), (3);
select * from #wf_insert_select;
update #wf_insert_select set id = 4;
select * from #wf_insert_select;
drop table #wf_insert_select;
