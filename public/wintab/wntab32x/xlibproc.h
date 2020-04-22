#define PROC_TEMPLATE2(proc, mod, internal, ordinal)			\
																\
																\
																\
	extern void  __ ## mod ## dynlink(PROCDATA *);				\
																\
	static PROCDATA p = {	#proc,								\
						ordinal,								\
						NULL,									\
						NULL									\
						};										\
																\
	typedef T_ ## proc (K_ ## proc * FT_ ## proc) P_ ## proc ;	\
																\
	T_ ## proc K_ ## proc proc P_ ## proc						\
	{															\
		if (p.fp==NULL)											\
			__ ## mod ## dynlink(&p);							\
		if (p.fp)												\
			return ((FT_ ## proc)p.fp) A_ ## proc;				\
		else													\
			return (T_ ## proc)0;								\
	}

#define PROC_TEMPLATE(proc, mod, internal, ordinal)		\
	PROC_TEMPLATE2(proc, mod, internal, ordinal)

