char create_table_sql[] = "

CREATE TABLE all_types_bcp_unittest (
	  not_null_bit			bit NOT NULL

	, not_null_char			char(10) NOT NULL
	, not_null_varchar		varchar(10) NOT NULL

	, not_null_datetime		datetime NOT NULL
	, not_null_smalldatetime	smalldatetime NOT NULL

	, not_null_money		money NOT NULL
	, not_null_smallmoney		smallmoney NOT NULL

	, not_null_float		float NOT NULL
	, not_null_real			real NOT NULL

	, not_null_decimal		decimal(5,2) NOT NULL
	, not_null_numeric		numeric(5,2) NOT NULL

	, not_null_int			int NOT NULL
	, not_null_smallint		smallint NOT NULL
	, not_null_tinyint		tinyint NOT NULL

	, nullable_bit			bit  NULL

	, nullable_char			char(10)  NULL
	, nullable_varchar		varchar(10)  NULL

	, nullable_datetime		datetime  NULL
	, nullable_smalldatetime	smalldatetime  NULL

	, nullable_money		money  NULL
	, nullable_smallmoney		smallmoney  NULL

	, nullable_float		float  NULL
	, nullable_real			real  NULL

	, nullable_decimal		decimal(5,2)  NULL
	, nullable_numeric		numeric(5,2)  NULL

	, nullable_int			int  NULL
	, nullable_smallint		smallint  NULL
	, nullable_tinyint		tinyint  NULL

	/* Excludes: 
	 * binary
	 * image
	 * uniqueidentifier
	 * varbinary
	 * text
	 * timestamp
	 * nchar
	 * ntext
	 * nvarchar
	 */
)

INSERT all_types_bcp_unittest   
VALUES ( 1 -- not_null_bit

	, 'a' -- not_null_char
	, 'a' -- not_null_varchar

	, 'Dec 17 2003  3:44PM' -- not_null_datetime
	, 'Dec 17 2003  3:44PM' -- not_null_smalldatetime

	, 12.34 -- not_null_money
	, 12.34 -- not_null_smallmoney

	, 12.34 -- not_null_float
	, 12.34 -- not_null_real

	, 12.34 -- not_null_decimal
	, 12.34 -- not_null_numeric

	, 1234 -- not_null_int
	, 1234 -- not_null_smallint
	, 123  -- not_null_tinyint

	, 1 -- nullable_bit

	, 'a' -- nullable_char
	, 'a' -- nullable_varchar

	, 'Dec 17 2003  3:44PM' -- nullable_datetime
	, 'Dec 17 2003  3:44PM' -- nullable_smalldatetime

	, 12.34 -- nullable_money
	, 12.34 -- nullable_smallmoney

	, 12.34 -- nullable_float
	, 12.34 -- nullable_real

	, 12.34 -- nullable_decimal
	, 12.34 -- nullable_numeric

	, 1234 -- nullable_int
	, 1234 -- nullable_smallint
	, 123  -- nullable_tinyint
)
INSERT all_types_bcp_unittest
				( not_null_bit			

				, not_null_char			
				, not_null_varchar		

				, not_null_datetime		
				, not_null_smalldatetime	

				, not_null_money		
				, not_null_smallmoney		

				, not_null_float		
				, not_null_real			

				, not_null_decimal		
				, not_null_numeric		

				, not_null_int			
				, not_null_smallint		
				, not_null_tinyint		
				) 
VALUES (
	  1 -- not_null_bit

	, 'a' -- not_null_char
	, 'a' -- not_null_varchar

	, 'Dec 17 2003  3:44PM' -- not_null_datetime
	, 'Dec 17 2003  3:44PM' -- not_null_smalldatetime

	, 12.34 -- not_null_money
	, 12.34 -- not_null_smallmoney

	, 12.34 -- not_null_float
	, 12.34 -- not_null_real

	, 12.34 -- not_null_decimal
	, 12.34 -- not_null_numeric

	, 1234 -- not_null_int
	, 1234 -- not_null_smallint
	, 123  -- not_null_tinyint
)
";