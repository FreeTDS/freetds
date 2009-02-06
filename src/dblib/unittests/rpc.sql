CREATE PROCEDURE #t0022 
  @null_input varchar(30) OUTPUT 
, @first_type varchar(30) OUTPUT 
, @nullout int OUTPUT
, @nrows int OUTPUT 
, @c varchar(20)
, @nv nvarchar(20) = N'hello'
AS 
BEGIN 
select @null_input = max(convert(varchar(30), name)) from systypes 
select @first_type = min(convert(varchar(30), name)) from systypes 
select name from sysobjects where 0=1
select distinct convert(varchar(30), name) as 'type'  from systypes 
where name in ('int', 'char', 'text') 
select @nrows = @@rowcount 
select distinct @nv as '@nv', convert(varchar(30), name) as name  from sysobjects where type = 'S' 
return 42 
END 

go
IF OBJECT_ID('t0022') IS NOT NULL DROP PROC t0022
go
CREATE PROCEDURE t0022 
  @null_input varchar(30) OUTPUT 
, @first_type varchar(30) OUTPUT 
, @nullout int OUTPUT
, @nrows int OUTPUT 
, @c varchar(20)
, @nv nvarchar(20) = N'hello'
AS 
BEGIN 
select @null_input = max(convert(varchar(30), name)) from systypes 
select @first_type = min(convert(varchar(30), name)) from systypes 
select name from sysobjects where 0=1
select distinct convert(varchar(30), name) as 'type'  from systypes 
where name in ('int', 'char', 'text') 
select @nrows = @@rowcount 
select distinct @nv as '@nv', convert(varchar(30), name) as name  from sysobjects where type = 'S' 
return 42 
END 

go
IF OBJECT_ID('t0022') IS NOT NULL DROP PROC t0022
go
