//
// NOTE: Source has a bunch of windows SDK headers checked in to the dx9sdk directory.
// This file is here to isolate dependencies on the newer version of the windows SDK
// NOTE: This code only applies to VISTA and greater

#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>

#define SAFE_RELEASE(punk)  \
	if ((punk) != NULL)  \
		{ (punk)->Release(); (punk) = NULL; }


extern unsigned int g_nDeviceStamp;

class CMMNotificationClient : public IMMNotificationClient
{
	LONG _cRef;
	IMMDeviceEnumerator *_pEnumerator;


public:
	CMMNotificationClient() :
	  _cRef(1),
		  _pEnumerator(NULL)
	  {
	  }

	  ~CMMNotificationClient()
	  {
		  SAFE_RELEASE(_pEnumerator)
	  }

	  // IUnknown methods -- AddRef, Release, and QueryInterface

	  ULONG STDMETHODCALLTYPE AddRef()
	  {
		  return InterlockedIncrement(&_cRef);
	  }

	  ULONG STDMETHODCALLTYPE Release()
	  {
		  ULONG ulRef = InterlockedDecrement(&_cRef);
		  if (0 == ulRef)
		  {
			  delete this;
		  }
		  return ulRef;
	  }

	  HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, VOID **ppvInterface)
	  {
		  if (IID_IUnknown == riid)
		  {
			  AddRef();
			  *ppvInterface = (IUnknown*)this;
		  }
		  else if (__uuidof(IMMNotificationClient) == riid)
		  {
			  AddRef();
			  *ppvInterface = (IMMNotificationClient*)this;
		  }
		  else
		  {
			  *ppvInterface = NULL;
			  return E_NOINTERFACE;
		  }
		  return S_OK;
	  }

	  // Callback methods for device-event notifications.

	  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged( EDataFlow flow, ERole /*role*/, LPCWSTR /*pwstrDeviceId*/ )
	  {
		  if ( flow == eRender )
		  {
			  g_nDeviceStamp++;
		  }
		  return S_OK;
	  }

	  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR /*pwstrDeviceId*/) { return S_OK; };
	  HRESULT STDMETHODCALLTYPE OnDeviceRemoved( LPCWSTR /*pwstrDeviceId*/ ) { return S_OK; }
	  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged( LPCWSTR /*pwstrDeviceId*/, DWORD /*dwNewState*/ ) { return S_OK; }
	  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged( LPCWSTR /*pwstrDeviceId*/, const PROPERTYKEY /*key*/ ) { return S_OK; }
};


CMMNotificationClient *g_pNotify = NULL;

HRESULT SetupWindowsMixerPreferences( bool bDuckingOptOut = true )
{
	HRESULT hr = S_OK;

	IMMDeviceEnumerator* pDeviceEnumerator = NULL;
	IMMDevice* pEndpoint = NULL;
	IAudioSessionManager2* pSessionManager2 = NULL;
	IAudioSessionControl* pSessionControl = NULL;
	IAudioSessionControl2* pSessionControl2 = NULL;


	//  Start with the default endpoint.

	hr = CoCreateInstance( __uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDeviceEnumerator) );

	if (SUCCEEDED(hr))
	{
		hr = pDeviceEnumerator->GetDefaultAudioEndpoint( eRender, eConsole, &pEndpoint);
		g_pNotify = new CMMNotificationClient;
		pDeviceEnumerator->RegisterEndpointNotificationCallback( g_pNotify );
		pDeviceEnumerator->Release();
		pDeviceEnumerator = NULL;
	}

	// Activate session manager.
	if (SUCCEEDED(hr))
	{
		hr = pEndpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&pSessionManager2) );
		pEndpoint->Release();
		pEndpoint = NULL;
	}

	if (SUCCEEDED(hr))
	{
		hr = pSessionManager2->GetAudioSessionControl(NULL, 0, &pSessionControl);
		// enable this code to force some default master volume for this game.  
		// NOTE: This will have the side effect of not remembering any setting the user made in the wiundows mixer
#if 0
		ISimpleAudioVolume *pSimpleVolume = NULL;
		hr = pSessionManager2->GetSimpleAudioVolume( NULL, FALSE, &pSimpleVolume );
		if ( SUCCEEDED(hr) )
		{
			pSimpleVolume->SetMasterVolume( flMasterVolume, NULL );
			pSimpleVolume->Release();
		}

#endif
		pSessionManager2->Release();
		pSessionManager2 = NULL;
	}

	if (SUCCEEDED(hr))
	{
		hr = pSessionControl->QueryInterface( __uuidof(IAudioSessionControl2), (void**)&pSessionControl2 );

		pSessionControl->Release();
		pSessionControl = NULL;
	}

	//  Sync the ducking state with the specified preference.

	if (SUCCEEDED(hr))
	{
		if (bDuckingOptOut)
		{
			hr = pSessionControl2->SetDuckingPreference(TRUE);
		}
		else
		{
			hr = pSessionControl2->SetDuckingPreference(FALSE);
		}
		pSessionControl2->Release();
		pSessionControl2 = NULL;
	}
	return hr;
}


bool GetWindowsDefaultAudioDevice( wchar_t *pDeviceNameOut, size_t nDeviceBufSize )
{
	IMMDeviceEnumerator* pDeviceEnumerator = NULL;
	IMMDevice* pEndpoint = NULL;

	bool bRet = false;
	//  Get the default audio endpoint from the multimedia API (more reliable than XAudio2 for example because it updates with dynamic changes to the default device)
	HRESULT hr = CoCreateInstance( __uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDeviceEnumerator) );

	if (SUCCEEDED(hr))
	{
		HRESULT hrEndPointDevice = pDeviceEnumerator->GetDefaultAudioEndpoint( eRender, eConsole, &pEndpoint );
		if (SUCCEEDED(hrEndPointDevice))
		{
			// now query the endpoint device for its Id
			LPWSTR deviceOut;
			if ( S_OK == pEndpoint->GetId( &deviceOut ) )
			{
				// got a valid Id, return true to the caller and save it
				wcsncpy_s( pDeviceNameOut, nDeviceBufSize/sizeof(wchar_t), deviceOut, _TRUNCATE );
				bRet = true;
				// GetId allocates memory when successful, we have to free it
				CoTaskMemFree( deviceOut );
			}
			pEndpoint->Release();
		}
		pDeviceEnumerator->Release();
	}
	return bRet;
}

class CInitializeCOM
{
public:
	CInitializeCOM()
	{
		CoInitializeEx(0, COINIT_MULTITHREADED);
	}

	~CInitializeCOM()
	{
		CoUninitialize();
	}
};

void InitCOM()
{
	static CInitializeCOM initializer;
}

void ShutdownCOM()
{
}

