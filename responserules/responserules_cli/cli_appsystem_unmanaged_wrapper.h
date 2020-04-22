/// The unmanaged side of wrapping the app system for CLR

#ifndef CLI_APPSYSTEM_UNMANAGED_WRAPPER_H
#define CLI_APPSYSTEM_UNMANAGED_WRAPPER_H


class CCLIAppSystemAdapter;

/// This is actually a manually implemented refcounter on 
/// a singleton instance, so that construction causes it to
/// be initialized if necessary and destruction refcounts
/// before NULLing the static global.
class AppSystemWrapper_Unmanaged
{
public:
	AppSystemWrapper_Unmanaged( const char *pCommandLine );
	virtual ~AppSystemWrapper_Unmanaged();


	inline int CountRefs( void ) const { return sm_nSingletonReferences; };
	inline CCLIAppSystemAdapter *operator *() const { return sm_pAppSystemSingleton; }
	inline operator CCLIAppSystemAdapter *()	const { return sm_pAppSystemSingleton; }
	inline static CCLIAppSystemAdapter *Get()		  { return sm_pAppSystemSingleton; }

protected:
	void InitializeAppSystem( CCLIAppSystemAdapter * pAppSys, const char *pCommandLine ) ;
	void TerminateAppSystem(  CCLIAppSystemAdapter * pAppSys ) ;

private:
	static CCLIAppSystemAdapter *sm_pAppSystemSingleton;
	static int sm_nSingletonReferences;
};





#endif