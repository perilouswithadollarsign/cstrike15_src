PROCD_STRUC struc
	pdPtr	dd ?
if UNLINK
	pdNext	dw ?
endif
PROCD_STRUC ends

PROCC_STRUC struc
	pcData		dw ?
if UNLINK
	pcLinkProc	dw ?
endif
	pcRetF		db 3 dup(?)
	pcStr		db ?
PROCC_STRUC ends


