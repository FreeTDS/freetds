if ((ret = tds_process_row_tokens(dbproc->tds_socket, &rowtype, &computeid))
    == TDS_SUCCEED) {
	if (rowtype == TDS_REG_ROW) {
		/* Add the row to the row buffer */
		resinfo = tds->curr_resinfo;
		buffer_add_row(&(dbproc->row_buf), resinfo->current_row,
					resinfo->row_size);
		result = REG_ROW;
	} else if (rowtype == TDS_COMP_ROW) {
		/* Add the row to the row buffer */
		resinfo = tds->curr_resinfo;
		buffer_add_row(&(dbproc->row_buf), resinfo->current_row,
					resinfo->row_size);
		result = computeid;
	} else
		result = FAIL;
} else if (ret == TDS_NO_MORE_ROWS) {
	result = NO_MORE_ROWS;
} else
	result = FAIL;
