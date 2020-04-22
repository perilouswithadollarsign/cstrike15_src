typedef struct _PROCDATA {
	LPCSTR				name;
	int					ord;
	FARPROC 			fp;
	struct _PROCDATA	*next;
} PROCDATA;
