MODULE_TEMPLATE MACRO mod, style, mod_list

	include cmacros.inc
	include xlib.inc

	fNOLOADLIBRARY = 0
	IFNB <style>
		IRP astyle,<style>
			IFIDN <astyle>,<NOLOADLIBRARY>
				fNOLOADLIBRARY = 1
			ENDIF
		ENDM
	ENDIF

	externFP GetProcAddress
	IFE fNOLOADLIBRARY
		externFP LoadLibrary
		externFP SetErrorMode
		IF UNLINK
			externFP FreeLibrary
		ENDIF
	ELSE
		externFP GetModuleHandle
	ENDIF

	sBegin Data
	globalW __h&mod, 0
	staticW procptr, 0
	sEnd Data

	sBegin Code
	assumes cs,Code
	assumes ds,Data

	IF fNOLOADLIBRARY
		IFNB <mod_list>
			__&mod&string label byte
			IRP amod, <mod_list>
				db "&amod&",0
			ENDM
			db 0
		ELSE
			__&mod&string db "&mod&",0
		ENDIF
	ELSE
		__&mod&string db "&mod&",0
		__&mod&file db "&mod&.dll",0
	ENDIF

	__&mod&dynlink proc far
		public __&mod&dynlink

	IF UNLINK
		mov ax, procptr
	ENDIF
		mov procptr, bx
	IF UNLINK
		mov bx, cs:[bx].pcData
		mov [bx].pdNext, ax
	ENDIF

		cmp __h&mod, 0
		jne get_proc

		farPtr mstr,cs,bx
	IF fNOLOADLIBRARY
		lea bx, __&mod&string
	mod_loop:
		save <bx, si, di, es>
		cCall GetModuleHandle,<mstr>
		or ax, ax
		jz no_link
	ELSE
		mov ax,08000h ;SEM_NOOPENFILEERRORBOX  0x8000
		save <si, di, es>
		cCall SetErrorMode,<ax>
		mov dx, ax 	; old error flags to DX

		lea bx, __&mod&file
		save <dx, si, di, es>
		cCall LoadLibrary,<mstr>

		xchg dx, ax	; old error flags to AX, LoadLibrary ret to DX
		save <dx, si, di, es>
		cCall SetErrorMode,<ax>

		cmp dx, 32
		jb no_link
	ENDIF
		mov __h&mod, ax

	get_proc:
		push bx		;; save ptr into module name list
		push es
		push cs
		pop es
		mov bx, procptr
		farPtr pstr,es,bx
		lea bx, cs:[bx].pcStr
		cmp byte ptr cs:[bx], 0
		jnz @f
		les bx, cs:[bx+1]
	@@:
		mov ax, __h&mod
		save <si, di>
		cCall GetProcAddress,<ax,pstr>
		push ax
		or ax,dx
		pop ax
		pop es
		pop bx		;; restore module name list ptr
		jz no_link
		mov bx, procptr
		mov bx, cs:[bx].pcData
		lea bx, [bx].pdPtr
		mov [bx], ax
		mov [bx+2], dx
		jmp dword ptr [bx]

	no_link:
	IF fNOLOADLIBRARY
		IFNB <mod_list>
		@@:
			cmp byte ptr cs:[bx], 0
			jz @f
			inc bx
			jmp short @b
		@@:
			inc bx
			cmp byte ptr cs:[bx], 0
			jz @f
			jmp mod_loop
		@@:
		ENDIF
	ENDIF
		xor ax, ax
		xor dx, dx
		mov bx, procptr
		lea bx, cs:[bx].pcRetF
		jmp bx

	__&mod&dynlink endp

	IF UNLINK
	__Unlink&mod	proc far
		public __Unlink&mod


		mov bx, __h&mod
		cmp bx, 0
		jz unlink_done
		mov __h&mod, 0

	IFE fNOLOADLIBRARY
		cCall FreeLibrary,<bx>
	ENDIF

	unlink_procs:
		mov	bx, procptr
	@@:
		cmp bx, 0
		jz unlink_done
		mov ax, cs:[bx].pcLinkProc
		mov bx, cs:[bx].pcData
		mov word ptr [bx].pdPtr, ax
		mov ax, cs
		mov word ptr [bx].pdPtr+2, ax
		mov bx, [bx].pdNext
		jmp @b

	unlink_done:
		mov procptr, 0
		ret

	__Unlink&mod	endp
	ENDIF

	sEnd Code
	end
ENDM
