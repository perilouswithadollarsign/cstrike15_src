PROC_TEMPLATE MACRO procname, retpop, mod, internal, ord

	.errb <retpop>		;; require non-blank

	include cmacros.inc
	include xlib.inc

	externFP __&mod&dynlink

	sBegin Data
	Data&procname PROCD_STRUC <LinkProc&procname>
	sEnd Data

	sBegin Code
	assumes cs,Code
	assumes ds,Data

	;; the data structure
	Ptr&procname label word

		.errnz pcData-($-Ptr&procname)
	dw DataOFFSET Data&procname

	IF UNLINK
			.errnz pcLinkProc-($-Ptr&procname)
		dw CodeOFFSET LinkProc&procname
	ENDIF

		.errnz pcRetF-($-Ptr&procname)
	bogus proc far
	IF retpop
		retf retpop*2
	ELSE
		retf
		dw 0
	ENDIF
	bogus endp

		.errnz pcStr-($-Ptr&procname)
	IFNB <ord>
		db 0
		dw ord, 0
	ELSE
		db "&procname&",0
	ENDIF


	;;  the dynlinking stub proc
	LinkProc&procname proc far
		lea bx, Ptr&procname
		jmp __&mod&dynlink
	LinkProc&procname endp

	;; the glue routine
	IFNB <internal>
		IFDIF <internal>,<->
		internal proc far
			public internal
			jmp Data&procname.pdPtr
		internal endp
		ELSE
			procname proc far
				public procname
				jmp Data&procname.pdPtr
			procname endp
		ENDIF
	ELSE
		procname proc far
			public procname
			jmp Data&procname.pdPtr
		procname endp
	ENDIF
	sEnd Code
	end
ENDM

