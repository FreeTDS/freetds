declare @a int
select @a = 123
begin transaction
go
/* The following statement should success */
select 634 as number
go
commit
go

