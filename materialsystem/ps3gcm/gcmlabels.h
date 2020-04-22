//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Labels etc..
//
//==================================================================================================

#ifndef INCLUDED_GCMLABELS_H
#define INCLUDED_GCMLABELS_H

enum GcmLabelEnum_t
{
	GCM_LABEL_QUERY_FIRST = 64,   // GCM reserves the first 64 labels, do not use them
	GCM_LABEL_QUERY_LAST  = GCM_LABEL_QUERY_FIRST + 99, // the last query label, inclusive

	GCM_LABEL_FPPATCH_RING_SEG = 252,
	GCM_LABEL_CALL_CMD_RING_SEG = 253,		// Ring command buffer for DrawPrimUP and similar
	GCM_LABEL_FLIP_CONTROL = 254,
	GCM_LABEL_MEMORY_FREE = 255				// 255 is the very last possible index of a label
};

enum GcmReportEnum_t
{
	// Used for occlusion queries
	GCM_REPORT_QUERY_FIRST = 0,
	GCM_REPORT_QUERY_LAST = GCM_REPORT_QUERY_FIRST + 512,

	// Used for RSX perf monitoring ... Four timestamps. Start and finish of this frame. Start and finish of previous frame
	GCM_REPORT_TIMESTAMP_FRAME_FIRST,
	GCM_REPORT_TIMESTAMP_FRAME_LAST = GCM_REPORT_TIMESTAMP_FRAME_FIRST + 3,

	// Used for Zcull stats
	GCM_REPORT_ZCULL_STATS_0,
	GCM_REPORT_ZCULL_STATS_1,
};

#endif // INCLUDED_GCMLABELS_H