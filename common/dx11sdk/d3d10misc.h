//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  File:       D3D10Misc.h
//  Content:    D3D10 Device Creation APIs
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __D3D10MISC_H__
#define __D3D10MISC_H__

#include "d3d10.h"

///////////////////////////////////////////////////////////////////////////
// ID3D10Blob:
// ------------
// The buffer object is used by D3D10 to return arbitrary size data.
//
// GetBufferPointer -
//    Returns a pointer to the beginning of the buffer.
//
// GetBufferSize -
//    Returns the size of the buffer, in bytes.
///////////////////////////////////////////////////////////////////////////

typedef interface ID3D10Blob ID3D10Blob;
typedef interface ID3D10Blob *LPD3D10BLOB;

// {8BA5FB08-5195-40e2-AC58-0D989C3A0102}
DEFINE_GUID(IID_ID3D10Blob, 
0x8ba5fb08, 0x5195, 0x40e2, 0xac, 0x58, 0xd, 0x98, 0x9c, 0x3a, 0x1, 0x2);

#undef INTERFACE
#define INTERFACE ID3D10Blob

DECLARE_INTERFACE_(ID3D10Blob, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3D10Blob
    STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
    STDMETHOD_(SIZE_T, GetBufferSize)(THIS) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

///////////////////////////////////////////////////////////////////////////
// D3D10_DRIVER_TYPE
// ----------------
//
// This identifier is used to determine the implementation of Direct3D10
// to be used.
//
// Pass one of these values to D3D10CreateDevice
//
///////////////////////////////////////////////////////////////////////////
typedef enum D3D10_DRIVER_TYPE
{
    D3D10_DRIVER_TYPE_HARDWARE  = 0,
    D3D10_DRIVER_TYPE_REFERENCE = 1,
    D3D10_DRIVER_TYPE_NULL      = 2,
    D3D10_DRIVER_TYPE_SOFTWARE  = 3,
    D3D10_DRIVER_TYPE_WARP      = 5,
} D3D10_DRIVER_TYPE;

DEFINE_GUID(GUID_DeviceType, 
0xd722fb4d, 0x7a68, 0x437a, 0xb2, 0x0c, 0x58, 0x04, 0xee, 0x24, 0x94, 0xa6);

///////////////////////////////////////////////////////////////////////////
// D3D10CreateDevice
// ------------------
//
// pAdapter
//      If NULL, D3D10CreateDevice will choose the primary adapter and 
//      create a new instance from a temporarily created IDXGIFactory.
//      If non-NULL, D3D10CreateDevice will register the appropriate 
//      device, if necessary (via IDXGIAdapter::RegisterDrver), before 
//      creating the device.
// DriverType
//      Specifies the driver type to be created: hardware, reference or 
//      null.
// Software
//      HMODULE of a DLL implementing a software rasterizer. Must be NULL for
//      non-Software driver types.
// Flags
//      Any of those documented for D3D10CreateDevice.
// SDKVersion
//      SDK version. Use the D3D10_SDK_VERSION macro.
// ppDevice
//      Pointer to returned interface.
//
// Return Values
//  Any of those documented for 
//          CreateDXGIFactory
//          IDXGIFactory::EnumAdapters
//          IDXGIAdapter::RegisterDriver
//          D3D10CreateDevice
//      
///////////////////////////////////////////////////////////////////////////
HRESULT WINAPI D3D10CreateDevice(
    IDXGIAdapter *pAdapter,
    D3D10_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    UINT SDKVersion,
    ID3D10Device **ppDevice);

///////////////////////////////////////////////////////////////////////////
// D3D10CreateDeviceAndSwapChain
// ------------------------------
//
// ppAdapter
//      If NULL, D3D10CreateDevice will choose the primary adapter and 
//      create a new instance from a temporarily created IDXGIFactory.
//      If non-NULL, D3D10CreateDevice will register the appropriate 
//      device, if necessary (via IDXGIAdapter::RegisterDrver), before 
//      creating the device.
// DriverType
//      Specifies the driver type to be created: hardware, reference or 
//      null.
// Software
//      HMODULE of a DLL implementing a software rasterizer. Must be NULL for
//      non-Software driver types.
// Flags
//      Any of those documented for D3D10CreateDevice.
// SDKVersion
//      SDK version. Use the D3D10_SDK_VERSION macro.
// pSwapChainDesc
//      Swap chain description, may be NULL.
// ppSwapChain
//      Pointer to returned interface. May be NULL.
// ppDevice
//      Pointer to returned interface.
//
// Return Values
//  Any of those documented for 
//          CreateDXGIFactory
//          IDXGIFactory::EnumAdapters
//          IDXGIAdapter::RegisterDriver
//          D3D10CreateDevice
//          IDXGIFactory::CreateSwapChain
//      
///////////////////////////////////////////////////////////////////////////
HRESULT WINAPI D3D10CreateDeviceAndSwapChain(
    IDXGIAdapter *pAdapter,
    D3D10_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    UINT SDKVersion,
    DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
    IDXGISwapChain **ppSwapChain,    
    ID3D10Device **ppDevice);


///////////////////////////////////////////////////////////////////////////
// D3D10CreateBlob:
// -----------------
// Creates a Buffer of n Bytes
//////////////////////////////////////////////////////////////////////////

HRESULT WINAPI D3D10CreateBlob(SIZE_T NumBytes, LPD3D10BLOB *ppBuffer);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__D3D10EFFECT_H__


