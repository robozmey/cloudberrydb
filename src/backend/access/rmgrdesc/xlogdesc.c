/*-------------------------------------------------------------------------
 *
 * xlogdesc.c
 *	  rmgr descriptor routines for access/transam/xlog.c
 *
 * Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/xlogdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "catalog/pg_control.h"
#include "utils/guc.h"
#include "utils/timestamp.h"

#include "access/twophase_xlog.h"
#include "cdb/cdbpublic.h"

/*
 * GUC support
 */
const struct config_enum_entry wal_level_options[] = {
	{"minimal", WAL_LEVEL_MINIMAL, false},
	{"replica", WAL_LEVEL_REPLICA, false},
	{"archive", WAL_LEVEL_REPLICA, true},	/* deprecated */
	{"hot_standby", WAL_LEVEL_REPLICA, true},	/* deprecated */
	{"logical", WAL_LEVEL_LOGICAL, false},
	{NULL, 0, false}
};


/*
 * This is used also in the redo function, but must be defined here so that it
 * can also be used in xlog_desc.
 */
void
UnpackCheckPointRecord(XLogReaderState *record, CheckpointExtendedRecord *ckptExtended)
{
	char *current_record_ptr;
	int remainderLen;

	if (XLogRecGetDataLen(record) == sizeof(CheckPoint))
	{
		/* Special (for bootstrap, xlog switch, maybe others) */
		ckptExtended->dtxCheckpoint = NULL;
		ckptExtended->dtxCheckpointLen = 0;
		return;
	}

	/* Normal checkpoint Record */
	Assert(XLogRecGetDataLen(record) > sizeof(CheckPoint));

	current_record_ptr = ((char*)XLogRecGetData(record)) + sizeof(CheckPoint);
	remainderLen = XLogRecGetDataLen(record) - sizeof(CheckPoint);

	/* Start of distributed transaction information */
	ckptExtended->dtxCheckpoint = (TMGXACT_CHECKPOINT *)current_record_ptr;
	ckptExtended->dtxCheckpointLen =
		TMGXACT_CHECKPOINT_BYTES((ckptExtended->dtxCheckpoint)->committedCount);

	Assert(remainderLen == ckptExtended->dtxCheckpointLen);
}

void
xlog_desc(StringInfo buf, XLogReaderState *record)
{
	char	   *rec = XLogRecGetData(record);
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

	if (info == XLOG_CHECKPOINT_SHUTDOWN ||
		info == XLOG_CHECKPOINT_ONLINE)
	{
		CheckPoint *checkpoint = (CheckPoint *) rec;

		CheckpointExtendedRecord ckptExtended;

		appendStringInfo(buf, "redo %X/%X; "
						 "tli %u; prev tli %u; fpw %s; xid %u:%u; gxid "UINT64_FORMAT"; oid %u; relfilenode %u; multi %u; offset %u; "
						 "oldest xid %u in DB %u; oldest multi %u in DB %u; "
						 "oldest/newest commit timestamp xid: %u/%u; "
						 "oldest running xid %u; %s",
						 LSN_FORMAT_ARGS(checkpoint->redo),
						 checkpoint->ThisTimeLineID,
						 checkpoint->PrevTimeLineID,
						 checkpoint->fullPageWrites ? "true" : "false",
						 EpochFromFullTransactionId(checkpoint->nextXid),
						 XidFromFullTransactionId(checkpoint->nextXid),
						 checkpoint->nextGxid,
						 checkpoint->nextOid,
						 checkpoint->nextRelfilenode,
						 checkpoint->nextMulti,
						 checkpoint->nextMultiOffset,
						 checkpoint->oldestXid,
						 checkpoint->oldestXidDB,
						 checkpoint->oldestMulti,
						 checkpoint->oldestMultiDB,
						 checkpoint->oldestCommitTsXid,
						 checkpoint->newestCommitTsXid,
						 checkpoint->oldestActiveXid,
						 (info == XLOG_CHECKPOINT_SHUTDOWN) ? "shutdown" : "online");

		UnpackCheckPointRecord(record, &ckptExtended);

		if (ckptExtended.dtxCheckpointLen > 0)
		{
			appendStringInfo(buf,
				 ", checkpoint record data length = %u, DTX committed count %d, DTX data length %u",
							 XLogRecGetDataLen(record),
							 ckptExtended.dtxCheckpoint->committedCount,
							 ckptExtended.dtxCheckpointLen);
		}
	}
	else if (info == XLOG_NEXTOID)
	{
		Oid			nextOid;

		memcpy(&nextOid, rec, sizeof(Oid));
		appendStringInfo(buf, "%u", nextOid);
	}
	else if (info == XLOG_NEXTGXID)
	{
		DistributedTransactionId nextGxid;

		nextGxid = *((DistributedTransactionId *) rec);
		appendStringInfo(buf, UINT64_FORMAT, nextGxid);
	}
	else if (info == XLOG_NEXTRELFILENODE)
	{
	    Oid   nextRelfilenode;

	    memcpy(&nextRelfilenode, rec, sizeof(Oid));
		appendStringInfo(buf, "%u", nextRelfilenode);
	}
	else if (info == XLOG_RESTORE_POINT)
	{
		xl_restore_point *xlrec = (xl_restore_point *) rec;

		appendStringInfoString(buf, xlrec->rp_name);
	}
	else if (info == XLOG_FPI || info == XLOG_FPI_FOR_HINT ||
			 info == XLOG_ENCRYPTION_LSN)
	{
		/* no further information to print */
	}
	else if (info == XLOG_BACKUP_END)
	{
		XLogRecPtr	startpoint;

		memcpy(&startpoint, rec, sizeof(XLogRecPtr));
		appendStringInfo(buf, "%X/%X", LSN_FORMAT_ARGS(startpoint));
	}
	else if (info == XLOG_PARAMETER_CHANGE)
	{
		xl_parameter_change xlrec;
		const char *wal_level_str;
		const struct config_enum_entry *entry;

		memcpy(&xlrec, rec, sizeof(xl_parameter_change));

		/* Find a string representation for wal_level */
		wal_level_str = "?";
		for (entry = wal_level_options; entry->name; entry++)
		{
			if (entry->val == xlrec.wal_level)
			{
				wal_level_str = entry->name;
				break;
			}
		}

		appendStringInfo(buf, "max_connections=%d max_worker_processes=%d "
						 "max_wal_senders=%d max_prepared_xacts=%d "
						 "max_locks_per_xact=%d wal_level=%s "
						 "wal_log_hints=%s track_commit_timestamp=%s",
						 xlrec.MaxConnections,
						 xlrec.max_worker_processes,
						 xlrec.max_wal_senders,
						 xlrec.max_prepared_xacts,
						 xlrec.max_locks_per_xact,
						 wal_level_str,
						 xlrec.wal_log_hints ? "on" : "off",
						 xlrec.track_commit_timestamp ? "on" : "off");
	}
	else if (info == XLOG_FPW_CHANGE)
	{
		bool		fpw;

		memcpy(&fpw, rec, sizeof(bool));
		appendStringInfoString(buf, fpw ? "true" : "false");
	}
	else if (info == XLOG_END_OF_RECOVERY)
	{
		xl_end_of_recovery xlrec;

		memcpy(&xlrec, rec, sizeof(xl_end_of_recovery));
		appendStringInfo(buf, "tli %u; prev tli %u; time %s",
						 xlrec.ThisTimeLineID, xlrec.PrevTimeLineID,
						 timestamptz_to_str(xlrec.end_time));
	}
	else if (info == XLOG_OVERWRITE_CONTRECORD)
	{
		xl_overwrite_contrecord xlrec;

		memcpy(&xlrec, rec, sizeof(xl_overwrite_contrecord));
		appendStringInfo(buf, "lsn %X/%X; time %s",
						 LSN_FORMAT_ARGS(xlrec.overwritten_lsn),
						 timestamptz_to_str(xlrec.overwrite_time));
	}
}

const char *
xlog_identify(uint8 info)
{
	const char *id = NULL;

	switch (info & ~XLR_INFO_MASK)
	{
		case XLOG_CHECKPOINT_SHUTDOWN:
			id = "CHECKPOINT_SHUTDOWN";
			break;
		case XLOG_CHECKPOINT_ONLINE:
			id = "CHECKPOINT_ONLINE";
			break;
		case XLOG_NOOP:
			id = "NOOP";
			break;
		case XLOG_NEXTOID:
			id = "NEXTOID";
			break;
		case XLOG_NEXTGXID:
			id = "NEXTGXID";
			break;
		case XLOG_NEXTRELFILENODE:
			id = "NEXTRELFILENODE";
			break;
		case XLOG_SWITCH:
			id = "SWITCH";
			break;
		case XLOG_BACKUP_END:
			id = "BACKUP_END";
			break;
		case XLOG_PARAMETER_CHANGE:
			id = "PARAMETER_CHANGE";
			break;
		case XLOG_RESTORE_POINT:
			id = "RESTORE_POINT";
			break;
		case XLOG_FPW_CHANGE:
			id = "FPW_CHANGE";
			break;
		case XLOG_END_OF_RECOVERY:
			id = "END_OF_RECOVERY";
			break;
		case XLOG_OVERWRITE_CONTRECORD:
			id = "OVERWRITE_CONTRECORD";
			break;
		case XLOG_FPI:
			id = "FPI";
			break;
		case XLOG_FPI_FOR_HINT:
			id = "FPI_FOR_HINT";
			break;
		case XLOG_ENCRYPTION_LSN:
			id = "ENCRYPTION_LSN";
			break;
	}

	return id;
}
