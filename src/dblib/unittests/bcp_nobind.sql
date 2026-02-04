if exists (select 1 from sysobjects where type = 'U' and name = 'bcp_nobind') drop table bcp_nobind
go
CREATE TABLE bcp_nobind
( i1 int null,
  i2 int default(7) NULL,
  i3 int default(9) NOT NULL
)
go
select * from bcp_nobind where i1 is null and i2 = 7 and i3 in (9, 13)
go
drop table bcp_nobind
go
