//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  File:       D3D11Shader.h
//  Content:    D3D11 Shader Types and APIs
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __D3D11SHADER_H__
#define __D3D11SHADER_H__

#include "d3d10_1shader.h"


typedef enum D3D11_RESOURCE_RETURN_TYPE
{
    D3D11_RETURN_TYPE_UNORM = 1,
    D3D11_RETURN_TYPE_SNORM = 2,
    D3D11_RETURN_TYPE_SINT = 3,
    D3D11_RETURN_TYPE_UINT = 4,
    D3D11_RETURN_TYPE_FLOAT = 5,
    D3D11_RETURN_TYPE_MIXED = 6,
    D3D11_RETURN_TYPE_DOUBLE = 7,
    D3D11_RETURN_TYPE_CONTINUED = 8,
} D3D11_RESOURCE_RETURN_TYPE;

typedef enum _D3D11_CBUFFER_TYPE
{
    D3D11_CT_CBUFFER,
    D3D11_CT_TBUFFER,
    D3D11_CT_INTERFACE_POINTERS,
    D3D11_CT_RESOURCE_BIND_INFO,
}  D3D11_CBUFFER_TYPE, *LPD3D11_CBUFFER_TYPE;


typedef struct _D3D11_SIGNATURE_PARAMETER_DESC
{
    LPCSTR                      SemanticName;   // Name of the semantic
    UINT                        SemanticIndex;  // Index of the semantic
    UINT                        Register;       // Number of member variables
    D3D10_NAME                  SystemValueType;// A predefined system value, or D3D10_NAME_UNDEFINED if not applicable
    D3D10_REGISTER_COMPONENT_TYPE ComponentType;// Scalar type (e.g. uint, float, etc.)
    BYTE                        Mask;           // Mask to indicate which components of the register
                                                // are used (combination of D3D10_COMPONENT_MASK values)
    BYTE                        ReadWriteMask;  // Mask to indicate whether a given component is 
                                                // never written (if this is an output signature) or
                                                // always read (if this is an input signature).
                                                // (combination of D3D10_COMPONENT_MASK values)
    UINT Stream;                                // Stream index
} D3D11_SIGNATURE_PARAMETER_DESC;

typedef struct _D3D11_SHADER_BUFFER_DESC
{
    LPCSTR                  Name;           // Name of the constant buffer
    D3D11_CBUFFER_TYPE      Type;           // Indicates type of buffer content
    UINT                    Variables;      // Number of member variables
    UINT                    Size;           // Size of CB (in bytes)
    UINT                    uFlags;         // Buffer description flags
} D3D11_SHADER_BUFFER_DESC;

typedef struct _D3D11_SHADER_VARIABLE_DESC
{
    LPCSTR                  Name;           // Name of the variable
    UINT                    StartOffset;    // Offset in constant buffer's backing store
    UINT                    Size;           // Size of variable (in bytes)
    UINT                    uFlags;         // Variable flags
    LPVOID                  DefaultValue;   // Raw pointer to default value
    UINT                    StartTexture;   // First texture index (or -1 if no textures used)
    UINT                    TextureSize;    // Number of texture slots possibly used.
    UINT                    StartSampler;   // First sampler index (or -1 if no textures used)
    UINT                    SamplerSize;    // Number of sampler slots possibly used.
} D3D11_SHADER_VARIABLE_DESC;

typedef struct _D3D11_SHADER_TYPE_DESC
{
    D3D10_SHADER_VARIABLE_CLASS Class;          // Variable class (e.g. object, matrix, etc.)
    D3D10_SHADER_VARIABLE_TYPE  Type;           // Variable type (e.g. float, sampler, etc.)
    UINT                        Rows;           // Number of rows (for matrices, 1 for other numeric, 0 if not applicable)
    UINT                        Columns;        // Number of columns (for vectors & matrices, 1 for other numeric, 0 if not applicable)
    UINT                        Elements;       // Number of elements (0 if not an array)
    UINT                        Members;        // Number of members (0 if not a structure)
    UINT                        Offset;         // Offset from the start of structure (0 if not a structure member)
    LPCSTR                      Name;           // Name of type, can be NULL
} D3D11_SHADER_TYPE_DESC;

typedef enum D3D11_TESSELLATOR_DOMAIN
{
    D3D11_TESSELLATOR_DOMAIN_UNDEFINED = 0,
    D3D11_TESSELLATOR_DOMAIN_ISOLINE   = 1,
    D3D11_TESSELLATOR_DOMAIN_TRI       = 2,
    D3D11_TESSELLATOR_DOMAIN_QUAD      = 3
} D3D11_TESSELLATOR_DOMAIN;

typedef enum D3D11_TESSELLATOR_PARTITIONING
{
    D3D11_TESSELLATOR_PARTITIONING_UNDEFINED       = 0,
    D3D11_TESSELLATOR_PARTITIONING_INTEGER         = 1,
    D3D11_TESSELLATOR_PARTITIONING_POW2            = 2,
    D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD  = 3,
    D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN = 4
} D3D11_TESSELLATOR_PARTITIONING;

typedef enum D3D11_TESSELLATOR_OUTPUT_PRIMITIVE
{
    D3D11_TESSELLATOR_OUTPUT_UNDEFINED    = 0,
    D3D11_TESSELLATOR_OUTPUT_POINT        = 1,
    D3D11_TESSELLATOR_OUTPUT_LINE         = 2,
    D3D11_TESSELLATOR_OUTPUT_TRIANGLE_CW  = 3,
    D3D11_TESSELLATOR_OUTPUT_TRIANGLE_CCW = 4
} D3D11_TESSELLATOR_OUTPUT_PRIMITIVE;

typedef struct _D3D11_SHADER_DESC
{
    UINT                    Version;                     // Shader version
    LPCSTR                  Creator;                     // Creator string
    UINT                    Flags;                       // Shader compilation/parse flags
    
    UINT                    ConstantBuffers;             // Number of constant buffers
    UINT                    BoundResources;              // Number of bound resources
    UINT                    InputParameters;             // Number of parameters in the input signature
    UINT                    OutputParameters;            // Number of parameters in the output signature

    UINT                    InstructionCount;            // Number of emitted instructions
    UINT                    TempRegisterCount;           // Number of temporary registers used 
    UINT                    TempArrayCount;              // Number of temporary arrays used
    UINT                    DefCount;                    // Number of constant defines 
    UINT                    DclCount;                    // Number of declarations (input + output)
    UINT                    TextureNormalInstructions;   // Number of non-categorized texture instructions
    UINT                    TextureLoadInstructions;     // Number of texture load instructions
    UINT                    TextureCompInstructions;     // Number of texture comparison instructions
    UINT                    TextureBiasInstructions;     // Number of texture bias instructions
    UINT                    TextureGradientInstructions; // Number of texture gradient instructions
    UINT                    FloatInstructionCount;       // Number of floating point arithmetic instructions used
    UINT                    IntInstructionCount;         // Number of signed integer arithmetic instructions used
    UINT                    UintInstructionCount;        // Number of unsigned integer arithmetic instructions used
    UINT                    StaticFlowControlCount;      // Number of static flow control instructions used
    UINT                    DynamicFlowControlCount;     // Number of dynamic flow control instructions used
    UINT                    MacroInstructionCount;       // Number of macro instructions used
    UINT                    ArrayInstructionCount;       // Number of array instructions used
    UINT                    CutInstructionCount;         // Number of cut instructions used
    UINT                    EmitInstructionCount;        // Number of emit instructions used
    D3D10_PRIMITIVE_TOPOLOGY GSOutputTopology;           // Geometry shader output topology
    UINT                    GSMaxOutputVertexCount;      // Geometry shader maximum output vertex count
    D3D11_PRIMITIVE         InputPrimitive;              // GS/HS input primitive
    UINT                    PatchConstantParameters;     // Number of parameters in the patch constant signature
    UINT                    cGSInstanceCount;            // Number of Geometry shader instances
    UINT                    cControlPoints;              // Number of control points in the HS->DS stage
    D3D11_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive; // Primitive output by the tessellator
    D3D11_TESSELLATOR_PARTITIONING HSPartitioning;       // Partitioning mode of the tessellator
    D3D11_TESSELLATOR_DOMAIN TessellatorDomain;          // Domain of the tessellator (quad, tri, isoline)
    // instruction counts
    UINT cBarrierInstructions;                           // Number of barrier instructions in a compute shader
    UINT cInterlockedInstructions;                       // Number of interlocked instructions
    UINT cTextureStoreInstructions;                      // Number of texture writes
} D3D11_SHADER_DESC;

typedef struct _D3D11_SHADER_INPUT_BIND_DESC
{
    LPCSTR                      Name;           // Name of the resource
    D3D10_SHADER_INPUT_TYPE     Type;           // Type of resource (e.g. texture, cbuffer, etc.)
    UINT                        BindPoint;      // Starting bind point
    UINT                        BindCount;      // Number of contiguous bind points (for arrays)
    
    UINT                        uFlags;         // Input binding flags
    D3D11_RESOURCE_RETURN_TYPE  ReturnType;     // Return type (if texture)
    D3D10_SRV_DIMENSION         Dimension;      // Dimension (if texture)
    UINT                        NumSamples;     // Number of samples (0 if not MS texture)
} D3D11_SHADER_INPUT_BIND_DESC;


//////////////////////////////////////////////////////////////////////////////
// Interfaces ////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

typedef interface ID3D11ShaderReflectionType ID3D11ShaderReflectionType;
typedef interface ID3D11ShaderReflectionType *LPD3D11SHADERREFLECTIONTYPE;

typedef interface ID3D11ShaderReflectionVariable ID3D11ShaderReflectionVariable;
typedef interface ID3D11ShaderReflectionVariable *LPD3D11SHADERREFLECTIONVARIABLE;

typedef interface ID3D11ShaderReflectionConstantBuffer ID3D11ShaderReflectionConstantBuffer;
typedef interface ID3D11ShaderReflectionConstantBuffer *LPD3D11SHADERREFLECTIONCONSTANTBUFFER;

typedef interface ID3D11ShaderReflection ID3D11ShaderReflection;
typedef interface ID3D11ShaderReflection *LPD3D11SHADERREFLECTION;

// {6E6FFA6A-9BAE-4613-A51E-91652D508C21}
DEFINE_GUID(IID_ID3D11ShaderReflectionType, 
0x6e6ffa6a, 0x9bae, 0x4613, 0xa5, 0x1e, 0x91, 0x65, 0x2d, 0x50, 0x8c, 0x21);

#undef INTERFACE
#define INTERFACE ID3D11ShaderReflectionType

DECLARE_INTERFACE(ID3D11ShaderReflectionType)
{
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_TYPE_DESC *pDesc) PURE;
    
    STDMETHOD_(ID3D11ShaderReflectionType*, GetMemberTypeByIndex)(THIS_ UINT Index) PURE;
    STDMETHOD_(ID3D11ShaderReflectionType*, GetMemberTypeByName)(THIS_ LPCSTR Name) PURE;
    STDMETHOD_(LPCSTR, GetMemberTypeName)(THIS_ UINT Index) PURE;

    STDMETHOD(IsEqual)(THIS_ ID3D11ShaderReflectionType* pType) PURE;
    STDMETHOD_(ID3D11ShaderReflectionType*, GetSubType)(THIS) PURE;
    STDMETHOD_(ID3D11ShaderReflectionType*, GetBaseClass)(THIS) PURE;
    STDMETHOD_(UINT, GetNumInterfaces)(THIS) PURE;
    STDMETHOD_(ID3D11ShaderReflectionType*, GetInterfaceByIndex)(THIS_ UINT uIndex) PURE;
    STDMETHOD(IsOfType)(THIS_ ID3D11ShaderReflectionType* pType) PURE;
    STDMETHOD(ImplementsInterface)(THIS_ ID3D11ShaderReflectionType* pBase) PURE;
};

// {51F23923-F3E5-4BD1-91CB-606177D8DB4C}
DEFINE_GUID(IID_ID3D11ShaderReflectionVariable, 
0x51f23923, 0xf3e5, 0x4bd1, 0x91, 0xcb, 0x60, 0x61, 0x77, 0xd8, 0xdb, 0x4c);

#undef INTERFACE
#define INTERFACE ID3D11ShaderReflectionVariable

DECLARE_INTERFACE(ID3D11ShaderReflectionVariable)
{
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_VARIABLE_DESC *pDesc) PURE;
    
    STDMETHOD_(ID3D11ShaderReflectionType*, GetType)(THIS) PURE;
    STDMETHOD_(ID3D11ShaderReflectionConstantBuffer*, GetBuffer)(THIS) PURE;

    STDMETHOD_(UINT, GetInterfaceSlot)(THIS_ UINT uArrayIndex) PURE;
};

// {EB62D63D-93DD-4318-8AE8-C6F83AD371B8}
DEFINE_GUID(IID_ID3D11ShaderReflectionConstantBuffer, 
0xeb62d63d, 0x93dd, 0x4318, 0x8a, 0xe8, 0xc6, 0xf8, 0x3a, 0xd3, 0x71, 0xb8);

#undef INTERFACE
#define INTERFACE ID3D11ShaderReflectionConstantBuffer

DECLARE_INTERFACE(ID3D11ShaderReflectionConstantBuffer)
{
    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_BUFFER_DESC *pDesc) PURE;
    
    STDMETHOD_(ID3D11ShaderReflectionVariable*, GetVariableByIndex)(THIS_ UINT Index) PURE;
    STDMETHOD_(ID3D11ShaderReflectionVariable*, GetVariableByName)(THIS_ LPCSTR Name) PURE;
};

// {17F27486-A342-4D10-8842-AB0874E7F670}
DEFINE_GUID(IID_ID3D11ShaderReflection, 
0x17f27486, 0xa342, 0x4d10, 0x88, 0x42, 0xab, 0x08, 0x74, 0xe7, 0xf6, 0x70);

#undef INTERFACE
#define INTERFACE ID3D11ShaderReflection

DECLARE_INTERFACE_(ID3D11ShaderReflection, IUnknown)
{
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    STDMETHOD(GetDesc)(THIS_ D3D11_SHADER_DESC *pDesc) PURE;
    
    STDMETHOD_(ID3D11ShaderReflectionConstantBuffer*, GetConstantBufferByIndex)(THIS_ UINT Index) PURE;
    STDMETHOD_(ID3D11ShaderReflectionConstantBuffer*, GetConstantBufferByName)(THIS_ LPCSTR Name) PURE;
    
    STDMETHOD(GetResourceBindingDesc)(THIS_ UINT ResourceIndex, D3D11_SHADER_INPUT_BIND_DESC *pDesc) PURE;
    
    STDMETHOD(GetInputParameterDesc)(THIS_ UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC *pDesc) PURE;
    STDMETHOD(GetOutputParameterDesc)(THIS_ UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC *pDesc) PURE;
    STDMETHOD(GetPatchConstantParameterDesc)(THIS_ UINT ParameterIndex, D3D11_SIGNATURE_PARAMETER_DESC *pDesc) PURE;

    STDMETHOD_(ID3D11ShaderReflectionVariable*, GetVariableByName)(THIS_ LPCSTR Name) PURE;

    STDMETHOD(GetResourceBindingDescByName)(THIS_ LPCSTR Name, D3D11_SHADER_INPUT_BIND_DESC *pDesc) PURE;

    STDMETHOD_(UINT, GetMovInstructionCount)(THIS) PURE;
    STDMETHOD_(UINT, GetMovcInstructionCount)(THIS) PURE;
    STDMETHOD_(UINT, GetConversionInstructionCount)(THIS) PURE;
    STDMETHOD_(UINT, GetBitwiseInstructionCount)(THIS) PURE;
    
    STDMETHOD_(D3D10_PRIMITIVE, GetGSInputPrimitive)(THIS) PURE;
    STDMETHOD_(BOOL, IsSampleFrequencyShader)(THIS) PURE;

    STDMETHOD_(UINT, GetNumInterfaceSlots)(THIS) PURE;
    STDMETHOD(GetMinFeatureLevel)(THIS_ enum D3D_FEATURE_LEVEL* pLevel) PURE;
};

//////////////////////////////////////////////////////////////////////////////
// APIs //////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#ifdef __cplusplus
}
#endif //__cplusplus
    
#endif //__D3D11SHADER_H__

