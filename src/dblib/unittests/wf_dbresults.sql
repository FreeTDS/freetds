create table #wf_dbresults(id int);
insert into #wf_dbresults values(1), (2), (3);
update #wf_dbresults set id = 1;
insert into #wf_dbresults values(2);
drop table #wf_dbresults;
