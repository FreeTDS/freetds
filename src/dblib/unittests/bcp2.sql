if exists (select 1 from sysobjects where type = 'U' and name = 'bcp_test') drop table bcp_test
go
CREATE TABLE bcp_test
( s1 char(10) not null,
  s2 char(10) not null,
  s3 char(10)  null )
go
select * from bcp_test where
	(s1 = 'test short' and s2 = '' and s3 is null)
	or
	(s1 = 'test short' and s2 = 'x' and s3 = '')
go
drop table bcp_test
go
