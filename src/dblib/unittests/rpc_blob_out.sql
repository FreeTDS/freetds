set textsize 60010
go
create proc #rpc_blob_out
  @txt varchar(max) out
as
set @txt=replicate(cast('pad-len-10' as varchar(max)),6000)+'test123'
return 0
go
