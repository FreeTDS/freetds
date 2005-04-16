static void buffer_struct_print(const DBPROC_ROWBUF *buf);

/** 
 * A few words on the buffering design.
 *
 * DBPROC_ROWBUF::buf is a block of row buffers, 
 * managed as a ring, indexed by head, tail, and current:
 *
 * head -- where new elements are inserted.
 * tail -- oldest element.
 * current -- active row (read by dbgetrow/dbnextrow)
 * 
 * capacity is the number of rows that buf can hold.
 * element_size is the size of each row.  
 *
 * Each element in buf is preceded by its row_number: 
 *  the result_set row number, determined by counting the rows
 *  as they're received from the server.  Applications communicate 
 *  to db-lib in row numbers, not buffer indices.  
 *
 * Semantics:
 * head == 0 && tail == capacity is the initial condition. 
 * head == tail means the buffer is full, except when capacity is 1. 
 * head < tail means the buffer has wrapped around.
 *
 * Whether or not buffering is active is governed by  
 * dbproc->dbopts[DBBUFFER].optactive.  
 */

/** 
 * number of rows in the buffer
 */
static int
buffer_count(const DBPROC_ROWBUF *buf)
{
	return (buf->head > buf->tail) ?
		buf->head - buf->tail :				/* |...TddddH....| */
		buf->capacity - (buf->tail - buf->head); 	/* |ddddH....Tddd| */
}
 
/** 
 * Can the buffer be written to?  
 */
static int
buffer_is_full(const DBPROC_ROWBUF *buf)
{
	return buf->capacity == buffer_count(buf) && buf->capacity > 1;
}

static int
buffer_index_valid(const DBPROC_ROWBUF *buf, int idx)
{
	if (buf->tail <= buf->head)
		if (buf->head <= idx && idx <= buf->tail)
			return 1;
	
	if (0 <= idx && idx <= buf->head)
		return 1;
	
	if (buf->tail <= idx && idx < buf->capacity)
		return 1;
#if 0	
	printf("buffer_index_valid: idx = %d\n", idx);
	buffer_struct_print(buf);
#endif
	return 0;	
}
 
/*
 * Buffer is freed at slightly odd points, whenever
 * capacity or element_size changes: 
 * 
 * 1. When setting capacity, to release prior buffer.  
 * 2. By dbresults.  When called the second time, it has to 
 * release prior storage because the new resultset will have
 * a different width.  
 * 3. By dbclose(), else open/close/open would leak.  
 */
static void
buffer_free(DBPROC_ROWBUF *buf)
{
	if (buf->rows != NULL)
		TDS_ZERO_FREE(buf->rows);
}

/*
 * When no rows are currently buffered (and the buffer is allocated)
 * set the indices to their initial postions.
 */
static void
buffer_reset(DBPROC_ROWBUF *buf)
{
	assert(buf->rows);
	
	buf->head = 0;
	buf->current = buf->tail = buf->capacity;
}

static int
buffer_idx_increment(const DBPROC_ROWBUF *buf, int idx)
{
	if (++idx >= buf->capacity) { 
		idx = 0;
	}
	return idx;
}

/**
 * Given an index, return the row storage, including
 * the DBINT row number prefix. 
 */ 
static void *
buffer_row_address(const DBPROC_ROWBUF * buf, int idx)
{
	const int offset = buf->element_size * idx;

	if (!(idx >= 0 && idx < buf->capacity)) {
		printf("idx is %d:\n", idx);
		buffer_struct_print(buf);
		assert(idx >= 0);
		assert(idx < buf->capacity);
	}
	
	return buf->rows + offset;
}

/**
 * Convert an index to a row number. 
 */ 
static DBINT
buffer_idx2row(const DBPROC_ROWBUF *buf, int idx)
{
	return *(DBINT*) buffer_row_address(buf, idx);
}

/**
 * Convert a row number to an index. 
 */ 
static int
buffer_row2idx(const DBPROC_ROWBUF *buf, int row_number)
{
	int i, ii, idx = -1;
	
	if (buf->tail == buf->capacity) {
		assert (buf->head == 0);
		return -1;	/* no rows buffered */
	}
	
	/* 
	 * March through the buffers from tail to head, stop if we find our row.  
	 * A full queue is indicated by tail == head (which means we can't write).
	 */
	for (ii=0, i = buf->tail; i != buf->head || ii == 0; i = buffer_idx_increment(buf, i)) {
		if( buffer_idx2row(buf, i) == row_number) {
			idx = i;
			break;
		}
		assert(ii++ < buf->capacity); /* prevent infinite loop */
	} 
	
	return idx;
}

/**
 * Deleting a row doesn't from the buffer doesn't affect 
 * memory allocation.  It just makes the space available 
 * for a different row.  
 */
static void
buffer_delete_rows(DBPROC_ROWBUF * buf,	int count)
{
	int i;

	if (count < 0 || count > buffer_count(buf)) {
		count = buffer_count(buf);
	}

	for (i=0; i < count; i++) {
		buf->tail = buffer_idx_increment(buf, buf->tail);
		/* 
		 * If deleting rows from the buffer catches the tail to the head, 
		 * return to the initial postion.  Otherwise, it will look full.
		 */
		if (buf->tail == buf->head) {
			buffer_reset(buf);
			break;
		}
	}
#if 0
	buffer_struct_print(buf);
#endif
}

static void
buffer_transfer_bound_data(DBPROC_ROWBUF *buf, TDS_INT res_type, TDS_INT compute_id, DBPROCESS * dbproc, int idx)
{

	int i;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int srctype;
	BYTE *src;
	int desttype;

	tds = dbproc->tds_socket;
	if (res_type == TDS_ROW_RESULT) {
		resinfo = tds->res_info;
	} else {		/* TDS_COMPUTE_RESULT */
		for (i=0; ;i++) {
			if (i >= tds->num_comp_info)
				return;
			resinfo = (TDSRESULTINFO *) tds->comp_info[i];
			if (resinfo->computeid == compute_id)
				break;
		}
	}

	for (i = 0; i < resinfo->num_cols; i++) {
		curcol = resinfo->columns[i];
		if (curcol->column_nullbind) {
			if (curcol->column_cur_size < 0) {
				*(DBINT *)(curcol->column_nullbind) = -1;
			} else {
				*(DBINT *)(curcol->column_nullbind) = 0;
			}
		}
		if (curcol->column_varaddr) {
			DBINT srclen;
			
			assert(buffer_index_valid(buf, idx));

			src = ((BYTE *) buffer_row_address(buf, idx)) + sizeof(DBINT) + curcol->column_offset;
			srclen = curcol->column_cur_size;
			if (is_blob_type(curcol->column_type)) {
				src = (BYTE *) ((TDSBLOB *) src)->textvalue;
			}
			desttype = _db_get_server_type(curcol->column_bindtype);
			srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);

			if (srclen < 0) {
				_set_null_value((BYTE *) curcol->column_varaddr, desttype, curcol->column_bindlen);
			} else {
				copy_data_to_host_var(dbproc, srctype, src, srclen, desttype, 
							(BYTE *) curcol->column_varaddr,  curcol->column_bindlen,
								 curcol->column_bindtype, curcol->column_nullbind);

			}
		}
	}
	
	/*
	 * This function always bumps current.  Usually, it's called 
	 * by dbnextrow(), so bumping current is a pretty obvious choice.  
	 * It can also be called by dbgetrow(), but that function also 
	 * causes the bump.  If you call dbgetrow() for row N, a subsequent
	 * call to dbnextrow() yields N+1.  
	 */
	buf->current = buffer_idx_increment(buf, buf->current);

}	/* end buffer_transfer_bound_data()  */

static void 
buffer_struct_print(const DBPROC_ROWBUF *buf)
{
	assert(buf);

	printf("\t%d rows in buffer\n", 	buffer_count(buf));
	
	printf("\thead = %d\t", 		buf->head);
	printf("\ttail = %d\t", 		buf->tail);
	printf("\tcurrent = %d\n", 		buf->current);
	printf("\tcapacity = %d\t", 		buf->capacity);
	printf("\telement_size = %d\n", 	buf->element_size);
	printf("\thead row number = %d\n", 	buf->received);
	printf("\tallocation = %x\n", 		buf->rows? buf->capacity * buf->element_size : 0);
}

/* * * Functions called only by public db-lib API take DBPROCESS* * */

/**
 * Return the current row buffer index.  
 * We strive to validate it first.  It must be:
 * 	between zero and capacity (obviously), and
 * 	between the head and the tail, logically.  
 *
 * If the head has wrapped the tail, it shouldn't be in no man's land.  
 * IOW, if capacity is 9, head is 3 and tail is 7, good rows are 7-8 and 0-2.
 *      (Row 3 is about-to-be-inserted, and 4-6 are not in use.)  Here's a diagram:
 * 		d d d ! ! ! ! d d
 *		0 1 2 3 4 5 6 7 8
 *		      ^       ^
 *		      Head    Tail
 *
 * The special case is capacity == 1, meaning there's no buffering, and head == tail === 0.  
 */
static int
buffer_current_index(const DBPROCESS *dbproc)
{
	const DBPROC_ROWBUF *buf = &dbproc->row_buf;
#if 0
	buffer_struct_print(buf);
#endif
	if (buf->capacity == 1) /* no buffering */
		return -1;
	if (buf->current == buf->head || buf->current == buf->capacity)
		return -1;
		
	assert(buf->current >= 0);
	assert(buf->current < buf->capacity);
	
	if( buf->tail < buf->head) {
		assert(buf->tail < buf->current);
		assert(buf->current < buf->head);
	} else {
		if (buf->current > buf->head)
	        	assert(buf->current > buf->tail);
	}
	return buf->current;
}

/*
 * Normally called by dbsetopt() to prepare for buffering
 * Called with nrows == 0 by dbopen to safely set buf->rows to NULL.  
 */
static void
buffer_set_capacity(DBPROCESS *dbproc, int nrows)
{
	DBPROC_ROWBUF *buf = &dbproc->row_buf;
	
	if (nrows > 0) {
		buffer_free(buf);
	}

	memset(buf, 0, sizeof(DBPROC_ROWBUF));

	if (0 == nrows) {
		buf->capacity = 1;
		return;
	}
	
	assert(0 < nrows);

	buf->capacity = nrows;
}

/*
 * Called only by dbresults(); capacity must be >= 1. 
 * Sybase's documents say dbresults() cannot return FAIL if the prior calls worked, 
 * which is a little strange, because (for FreeTDS, at least), dbresults
 * is when we learn about the result set's width.  Without that information, we
 * can't allocate memory for the buffer.  But if we *fail* to allocate memory, 
 * we're not to communicate it back to the caller?   
 */
static void
buffer_alloc(DBPROCESS *dbproc, int element_size)
{
	DBPROC_ROWBUF *buf = &dbproc->row_buf;
	
	/* Call this function only after setting capacity. */

	assert(buf);
	assert(element_size > 0);
	assert(buf->capacity > 0);
	assert(buf->rows == NULL);
	
	/* align rows on DBINT boundaries */
	if (element_size % sizeof(DBINT))
		element_size += sizeof(DBINT) - element_size % sizeof(DBINT);
		
	element_size += sizeof(DBINT);
	
	buf->rows = calloc(buf->capacity, element_size);
	
	assert(buf->rows);
	
	buffer_reset(buf);
	
	buf->received = 0;
	buf->element_size = element_size;
}

/**
 * Called by dbnextrow
 * Returns a row buffer index, or -1 to indicate the buffer is full.
 */
static int
buffer_add_row(DBPROCESS *dbproc, void *row, int row_size)
{
	DBPROC_ROWBUF *buf = &dbproc->row_buf;
	
	BYTE *dest = NULL;

	assert(row_size > 0);
	assert(row_size <= buf->element_size);

	assert(buf->capacity >= 1);
	
	if (buffer_is_full(buf))
		return -1;

	/* initial condition is head == 0 and tail == capacity */
	if (buf->tail == buf->capacity) {
		/* bumping this tail will set it to zero */
		assert(buf->head == 0);
		buf->tail = buffer_idx_increment(buf, buf->tail);
	}
	
	dest = (BYTE*) buffer_row_address(buf, buf->head);

	/* bump the row number, write it, and move the data to head */
	*(DBINT*) dest = ++buf->received;
	memcpy(dest + sizeof(DBINT), row, row_size);
	
	/* update current, bump the head */
	buf->current = buf->head;
	buf->head = buffer_idx_increment(buf, buf->head);
	
	return buf->current;
}


