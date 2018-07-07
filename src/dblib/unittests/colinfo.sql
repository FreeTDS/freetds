create table #colinfo_table(is_an_int int not null, is_a_string char(10) not null)
create table #test_table(is_a_money money not null)
go
select is_an_int as number, is_a_string, is_a_money as dollars from #colinfo_table, #test_table for browse
go
