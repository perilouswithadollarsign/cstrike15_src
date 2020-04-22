#define MODULE_TEMPLATE2(mod, fname)									\
																		\
																		\
	static HMODULE hMod = NULL;											\
	static PROCDATA *pList = NULL;										\
																		\
	static TCHAR szMod[] = TEXT( #fname );								\
																		\
	void  __ ## mod ## dynlink(PROCDATA *p)								\
	{																	\
		PROCDATA *head;													\
																		\
		if (!hMod) {													\
			UINT errmode = SetErrorMode(SEM_NOOPENFILEERRORBOX			\
								| SEM_FAILCRITICALERRORS);				\
			hMod = LoadLibrary(szMod);									\
			SetErrorMode(errmode);										\
		}																\
		if (hMod) {														\
			if (p->ord)													\
				p->fp = GetProcAddress(hMod, (LPCSTR)(p->ord));			\
			else														\
				p->fp = GetProcAddress(hMod, p->name);					\
		}																\
		if (p->fp) {													\
			head = pList;												\
			pList = p;													\
			p->next = head;												\
		}																\
	}																	\
																		\
	void __ ## mod ## unlink(void)										\
	{																	\
		if (hMod) {														\
			FreeLibrary(hMod);					   						\
			hMod = NULL;						   						\
												   						\
			while(pList) {						   						\
				PROCDATA *head;											\
				pList->fp = NULL;										\
				head = pList->next;										\
				pList->next = NULL;										\
				pList = head;											\
			}															\
		}																\
	}																	\
																		\
	void _Unlink ## mod(void) {__ ## mod ## unlink();}

#define MODULE_TEMPLATE(mod, fname) MODULE_TEMPLATE2(mod, fname)
