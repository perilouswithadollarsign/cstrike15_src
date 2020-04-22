 /* = NULL */
//
//=============================================================================


// Valve includes
#include "filesystem.h"
#include "fbxsystem/ifbxsystem.h"
#include "fbxutils/dmfbxserializer.h"
#include "movieobjects/dmedccmakefile.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmeaxissystem.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeexporttags.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmemodel.h"
#include "resourcefile/resourcedictionary.h"
#ifdef SOURCE2
#include "resourcesystem/resourcehandletypes.h"
#endif
#include "tier1/fmtstr.h"


// Last include
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFbxAxisSystem : public FbxAxisSystem
{
public:
	CDmeFbxAxisSystem(
		CDmeAxisSystem::Axis_t eUpAxis,
		CDmeAxisSystem::ForwardParity_t eForwardParity,
		CDmeAxisSystem::CoordSys_t eCoordSys )
	: FbxAxisSystem(
		static_cast< EUpVector >( eUpAxis ),
		static_cast< EFrontVector >( eForwardParity ),
		static_cast< ECoordSystem >( eCoordSys ) )
	{
	}

	CDmeFbxAxisSystem( EUpVector eUpVector, EFrontVector eFrontVector, ECoordSystem eCoordSystem )
	: FbxAxisSystem( eUpVector, eFrontVector, eCoordSystem )
	{
	}

	CDmeFbxAxisSystem( const FbxAxisSystem &rhs )
	{
		FbxAxisSystem::operator=( rhs );
	}

	CDmeFbxAxisSystem &operator=( const FbxAxisSystem &rhs )
	{
		FbxAxisSystem::operator=( rhs );
		return *this;
	}

	CUtlString GetAxes() const
	{
		int nUSign = 0;
		const int nU = GetUpAxis( nUSign );
		int nFSign = 0;
		const int nF = GetFrontAxis( nFSign );
		int nLSign = 0;
		const int nL = GetLeftAxis( nLSign );

		const char *szAxis[] = { "X", "Y", "Z" };

		return CUtlString( CFmtStr( "U: %s%s F: %s%s L: %s%s",
			nUSign < 0 ? "-" : " ", szAxis[nU],
			nFSign < 0 ? "-" : " ", szAxis[nF],
			nLSign < 0 ? "-" : " ", szAxis[nL] ).Get() );
	}

	CUtlString PrintRot( const FbxAxisSystem &from ) const
	{
		FbxMatrix mFbx;
		GetConversionMatrix( from, mFbx );
		FbxVector4 vT;
		FbxQuaternion qR;
		FbxVector4 vSh;
		FbxVector4 vSc;
		double flSign;
		mFbx.GetElements( vT, qR, vSh, vSc, flSign );
		FbxVector4 vR = qR.DecomposeSphericalXYZ();

		mFbx = mFbx.Inverse();
		mFbx.GetElements( vT, qR, vSh, vSc, flSign );
		FbxVector4 vI = qR.DecomposeSphericalXYZ();
		return CUtlString( CFmtStr( "F < %6.2f %6.2f %6.2f > I < %6.2f %6.2f %6.2f >\n",
			RAD2DEG( vR[0] ),
			RAD2DEG( vR[1] ),
			RAD2DEG( vR[2] ),
			RAD2DEG( vI[0] ),
			RAD2DEG( vI[1] ),
			RAD2DEG( vI[2] ) ).Get() );
	}

	void GetConversionRotation( RadianEuler &e, const FbxAxisSystem &from )
	{
		FbxMatrix mFbx;
		GetConversionMatrix( from, mFbx );
		mFbx = mFbx.Inverse();
		FbxVector4 vT;
		FbxQuaternion qR;
		FbxVector4 vSh;
		FbxVector4 vSc;
		double flSign;
		mFbx.GetElements( vT, qR, vSh, vSc, flSign );
		FbxVector4 vR = qR.DecomposeSphericalXYZ();
		e.x = vR[0];
		e.y = vR[1];
		e.z = vR[2];
	}

	int GetUpAxis( int &nSign ) const { nSign = mUpVector.mSign; return mUpVector.mAxis; };
	int GetFrontAxis( int &nSign ) const { nSign = mFrontVector.mSign; return mFrontVector.mAxis; };
	int GetLeftAxis( int &nSign ) const { nSign = mCoorSystem.mSign; return mCoorSystem.mAxis; };

//	static void Validate();
};


#if 0
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFbxAxisSystem::Validate()
{
	const int fbxUp[] = { -FbxAxisSystem::eXAxis, -FbxAxisSystem::eYAxis, -FbxAxisSystem::eZAxis, FbxAxisSystem::eXAxis, FbxAxisSystem::eYAxis, FbxAxisSystem::eZAxis };
	const int fbxParity[] = { -FbxAxisSystem::eParityEven, -FbxAxisSystem::eParityOdd, FbxAxisSystem::eParityEven, FbxAxisSystem::eParityOdd };

	const int dmxUp[] = { CDmeAxisSystem::AS_AXIS_NX, CDmeAxisSystem::AS_AXIS_NY, CDmeAxisSystem::AS_AXIS_NZ, CDmeAxisSystem::AS_AXIS_X, CDmeAxisSystem::AS_AXIS_Y, CDmeAxisSystem::AS_AXIS_Z };
	const int dmxParity[] = { CDmeAxisSystem::AS_PARITY_NEVEN, CDmeAxisSystem::AS_PARITY_NODD, CDmeAxisSystem::AS_PARITY_EVEN, CDmeAxisSystem::AS_PARITY_ODD };

	COMPILE_TIME_ASSERT( ARRAYSIZE( fbxUp ) == ARRAYSIZE( dmxUp ) );
	COMPILE_TIME_ASSERT( ARRAYSIZE( fbxParity ) == ARRAYSIZE( dmxParity ) );

	int nOkCount = 0;
	int nBadCount = 0;

	for ( int i = 0; i < ARRAYSIZE( fbxUp ); ++i )
	{
		const int nFbxAU = fbxUp[i];
		const int nDmxAU = dmxUp[i];

		Assert( nFbxAU == nDmxAU );

		for ( int j = 0; j < ARRAYSIZE( fbxParity ); ++j )
		{
			const int nFbxAP = fbxParity[j];
			const int nDmxAP = dmxParity[j];

			Assert( nFbxAP == nDmxAP );

			// A = FROM
			CDmeFbxAxisSystem fA(
				static_cast< FbxAxisSystem::EUpVector >( nFbxAU ),
				static_cast< FbxAxisSystem::EFrontVector >( nFbxAP ),
				FbxAxisSystem::eRightHanded );

			const CDmeAxisSystem::Axis_t eAU = static_cast< CDmeAxisSystem::Axis_t >( nDmxAU );
			const CDmeAxisSystem::ForwardParity_t eAF = static_cast< CDmeAxisSystem::ForwardParity_t >( nDmxAP );
			const CDmeAxisSystem::CoordSys_t eAC = CDmeAxisSystem::AS_RIGHT_HANDED;

			for ( int k = 0; k < ARRAYSIZE( fbxUp ); ++k )
			{
				const int nFbxBU = fbxUp[k];
				const int nDmxBU = dmxUp[k];

				Assert( nFbxBU == nDmxBU );

				for ( int l = 0; l < ARRAYSIZE( fbxParity ); ++l )
				{
					const int nFbxBP = fbxParity[l];
					const int nDmxBP = dmxParity[l];

					Assert( nFbxBP == nDmxBP );

					// B = TO
					CDmeFbxAxisSystem fB(
						static_cast< FbxAxisSystem::EUpVector >( nFbxBU ),
						static_cast< FbxAxisSystem::EFrontVector >( nFbxBP ),
						FbxAxisSystem::eRightHanded );

					const CDmeAxisSystem::Axis_t eBU = static_cast< CDmeAxisSystem::Axis_t >( nDmxBU );
					const CDmeAxisSystem::ForwardParity_t eBF = static_cast< CDmeAxisSystem::ForwardParity_t >( nDmxBP );
					const CDmeAxisSystem::CoordSys_t eBC = CDmeAxisSystem::AS_RIGHT_HANDED;

					RadianEuler eFbx;
					fB.GetConversionRotation( eFbx, fA );

					if ( i == k && j == l )
					{
						Assert( RadianEulersAreEqual( eFbx, RadianEuler( 0.0f, 0.0f, 0.0f ), 1.0e-6 ) );
					}

					matrix3x4a_t mDmx;
					CDmeAxisSystem::GetConversionMatrix( mDmx,
						eBU, eBF, eBC,
						eAU, eAF, eAC );
					RadianEuler eDmx;
					MatrixAngles( mDmx, eDmx );

					if ( i == k && j == l )
					{
						Assert( RadianEulersAreEqual( eDmx, RadianEuler( 0.0f, 0.0f, 0.0f ), 1.0e-6 ) );
					}

					// Account for FBX/DMX differences in converting matrix to Euler
					Quaternion qFbx = eFbx;
					QuaternionNormalize( qFbx );
					Quaternion qDmx = eDmx;
					QuaternionNormalize( qDmx );
					eFbx = qFbx;
					eDmx = qDmx;

					if ( !QuaternionsAreEqual( qFbx, qDmx, 1.0e-6 ) )
					{
						Msg( " * FBX \"%s\" -> \"%s\": %6.2f %6.2f %6.2f\n", fA.GetAxes().Get(), fB.GetAxes().Get(),
							RAD2DEG( eFbx.x ),
							RAD2DEG( eFbx.y ),
							RAD2DEG( eFbx.z ) );
						Msg( " * DMX \"%s\" -> \"%s\": %6.2f %6.2f %6.2f\n",
							CDmeAxisSystem::GetAxisString( eAU, eAF, eAC ).Get(),
							CDmeAxisSystem::GetAxisString( eBU, eBF, eBC ).Get(),
							RAD2DEG( eDmx.x ),
							RAD2DEG( eDmx.y ),
							RAD2DEG( eDmx.z ) );
						Msg( "\n" );

						++nBadCount;
					}
					else
					{
						++nOkCount;
					}

				}
			}
		}
	}

	Msg( " * OK: %4d Bad: %d\n", nOkCount, nBadCount );
}
#endif // 0


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *FbxImporterGetErrorString( FbxImporter *pFbxImporter )
{
#if FBXSDK_VERSION_MAJOR >= 2014
	return pFbxImporter->GetStatus().GetErrorString();
#else // FBXSDK_VERSION_MAJOR
	return pFbxImporter->GetLastErrorString();
#endif // FBXSDK_VERSION_MAJOR
}


//=============================================================================
//
// CDmFbxSerializer
//
//=============================================================================


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmFbxSerializer::CDmFbxSerializer()
: m_nOptVerbosity( 0 )
, m_bOptUnderscoreForCorrectors( false )
, m_bAnimation( false )
, m_bReturnDmeModel( false )
, m_flOptScale( 1.0f )
{
	// Initialize Axis System To Maya Y Up
	CDmeAxisSystem::GetPredefinedAxisSystem( m_eOptUpAxis, m_eOptForwardParity, m_eCoordSys, CDmeAxisSystem::AS_MAYA_YUP );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmFbxSerializer::~CDmFbxSerializer()
{
}


//-----------------------------------------------------------------------------
// Common function both ReadFBX & Unserialize can call
//-----------------------------------------------------------------------------
CDmElement *CDmFbxSerializer::ReadFBX( const char *pszFilename )
{
	FbxManager *pFbxManager = GetFbxManager();
	if ( !pFbxManager )
		return NULL;

	DmFileId_t nDmFileId = g_pDataModel->FindOrCreateFileId( pszFilename );

	if ( nDmFileId == DMFILEID_INVALID )
	{
		Warning( "Warning! Couldn't create DmFileId_t for \"%s\"\n", pszFilename );
		return NULL;
	}

	FbxTime::EMode eFbxTimeMode = FbxTime::eFrames30;

	FbxScene *pFbxScene = LoadFbxScene( eFbxTimeMode, pszFilename );
	if ( !pFbxScene )
		return NULL;

	if ( !FloatsAreEqual( m_flOptScale, 1.0f, 1.0e-4 ) )
	{
		FbxSystemUnit fbxSystemUnit( 1.0 / m_flOptScale );
		fbxSystemUnit.ConvertScene( pFbxScene );
	}

	CDmeFbxAxisSystem fromAxisSystem = pFbxScene->GetGlobalSettings().GetAxisSystem();
	CDmeFbxAxisSystem toAxisSystem( m_eOptUpAxis, m_eOptForwardParity, m_eCoordSys );
	toAxisSystem.ConvertScene( pFbxScene );

	CDmeAxisSystem::Axis_t eUpAxis;
	CDmeAxisSystem::ForwardParity_t eForwardParity;
	CDmeAxisSystem::CoordSys_t eCoordSys;

	{
		// nU [ -1, 1 ], eU [ 1, 2, 3 ]
		int nU = 0;
		const int eU = fromAxisSystem.GetUpVector( nU );
		eUpAxis = static_cast< CDmeAxisSystem::Axis_t >( nU * eU );

		// nF [ -1, 1 ], eF [ 1, 2 ]
		int nF = 0;
		const int eF = fromAxisSystem.GetFrontVector( nF );
		eForwardParity = static_cast< CDmeAxisSystem::ForwardParity_t >( nF * eF );

		// GetCoordSys() [ 0, 1 ]
		eCoordSys = static_cast< CDmeAxisSystem::CoordSys_t >( fromAxisSystem.GetCoorSystem() );
	}

	char szFileBase[ MAX_PATH ] = "";
	V_FileBase( pszFilename, szFileBase, ARRAYSIZE( szFileBase ) );

	CDmElement *pDmeRoot = NULL;
	CDmeModel *pDmeModel = CreateElement< CDmeModel >( szFileBase, nDmFileId );
#ifdef SOURCE2
	pDmeModel->SetAxisSystem( m_eOptUpAxis, m_eOptForwardParity, m_eCoordSys );
#endif	
	if ( !m_bAnimation && m_bReturnDmeModel )
	{
		pDmeRoot = pDmeModel;
	}
	else
	{
		pDmeRoot = CreateElement< CDmElement >( "root", nDmFileId );
		pDmeRoot->SetValue( "skeleton", pDmeModel );

		if ( !m_bAnimation )
		{
			// Don't set "model" if animation only... Studiomdl can't handle it
			pDmeRoot->SetValue( "model", pDmeModel );
		}
	}

	g_pDataModel->SetFileRoot( nDmFileId, pDmeRoot->GetHandle() );

	CDmeDCCMakefile *pDmeMakefile = CreateElement< CDmeDCCMakefile >( "makefile", nDmFileId );
	pDmeRoot->SetValue( pDmeMakefile->GetName(), pDmeMakefile );

	// DMX sources are relative to the DMX file

	char szFullPath[ MAX_PATH ];
	if ( g_pFullFileSystem->RelativePathToFullPath( pszFilename, NULL, szFullPath, ARRAYSIZE( szFullPath ) ) )
	{
		pDmeMakefile->AddSource< CDmeSource >( szFullPath );
	}
	else
	{
		pDmeMakefile->AddSource< CDmeSource >( pszFilename );
	}

	FbxDocumentInfo *pFbxDocumentInfo = pFbxScene->GetSceneInfo();
	if ( pFbxDocumentInfo )
	{
		FbxString sUrl = pFbxDocumentInfo->LastSavedUrl.Get();
		FbxString sOrigFilename = pFbxDocumentInfo->Original_FileName.Get();

		char szUrl[MAX_PATH] = {};
		char szOrigFilename[MAX_PATH] = {};

		V_FixupPathName( szUrl, ARRAYSIZE( szUrl ), sUrl.Buffer() );
		V_FixSlashes( szUrl, '/' );
		V_FixupPathName( szOrigFilename, ARRAYSIZE( szOrigFilename ), sOrigFilename.Buffer() );
		V_FixSlashes( szOrigFilename, '/' );

		if ( V_strcmp( szUrl, szOrigFilename ) != 0 )
		{
#ifdef SOURCE2
			char szRelativePath[MAX_PATH] = {};
			GenerateResourceNameFromFileName( sOrigFilename.Buffer(), szRelativePath, ARRAYSIZE( szRelativePath ) );

			if ( V_strlen( szRelativePath ) > 0 && GenerateStandardFullPathForResourceName( szRelativePath, RESOURCE_PATH_CONTENT, szFullPath, ARRAYSIZE( szFullPath ) ) )
			{
				pDmeMakefile->AddSource< CDmeSource >( szFullPath );
			}
			else
#endif
			{
				pDmeMakefile->AddSource< CDmeSource >( sOrigFilename.Buffer() );
			}
		}
	}

	CDmeExportTags *pDmeExportTags = CreateElement< CDmeExportTags >( "exportTags", nDmFileId );
	pDmeExportTags->Init( "fbx2dmx", g_pFbx->GetFbxManager()->GetVersion() );
	pDmeExportTags->SetValue( "cmdLine", CommandLine()->GetCmdLine() );
	{
		char szCurrentDirectory[ MAX_PATH ];
		Plat_getwd( szCurrentDirectory, ARRAYSIZE( szCurrentDirectory ) );
		V_FixSlashes( szCurrentDirectory, '/' );
		pDmeExportTags->SetValue( "pwd", szCurrentDirectory );
	}

	pDmeRoot->SetValue( pDmeExportTags->GetName(), pDmeExportTags );

	CDmAttribute *pRootAttr = pDmeModel->AddAttribute( "__rootElement", AT_ELEMENT );
	if ( pRootAttr )
	{
		pRootAttr->AddFlag( FATTRIB_DONTSAVE );
		pRootAttr->SetValue( pDmeRoot );
	}

	FbxToDmxMap_t fbxToDmxMap( CDefOps< FbxToDmxMap_t::KeyType_t >::LessFunc );

	// Don't create a DmeDag for the root node
	FbxNode *pFbxRootNode = pFbxScene->GetRootNode();

	if ( Verbose2() )
	{
		Msg( " * Skeleton\n" );
	}

	for ( int i = 0; i < pFbxRootNode->GetChildCount(); ++i )
	{
		LoadModelAndSkeleton_R( fbxToDmxMap, pDmeModel, pDmeModel, pFbxRootNode->GetChild( i ), m_bAnimation, 0 );
	}

	pDmeModel->CaptureJointsToBaseState( "bind" );

	if ( m_bAnimation )
	{
		LoadAnimation( pDmeRoot, pDmeModel, fbxToDmxMap, pFbxScene, pFbxRootNode, eFbxTimeMode );
	}
	else
	{
		for ( int i = 0; i < pFbxRootNode->GetChildCount(); ++i )
		{
			SkinMeshes_R( fbxToDmxMap, pDmeModel, pFbxRootNode->GetChild( i ) );
			AddBlendShapes_R( fbxToDmxMap, pDmeRoot, pFbxRootNode->GetChild( i ) );
		}
	}

	pFbxScene->Destroy();

	pDmeModel->ConvertToAxisSystem( CDmeAxisSystem::AS_VALVE_ENGINE );

	return pDmeRoot;
}


//-----------------------------------------------------------------------------
// Feed the CDmElement returned by ReadFBX to see if there were non-fatal conversion errors which the user should be informed about
//-----------------------------------------------------------------------------
bool CDmFbxSerializer::HasConversionErrors( CDmElement *pDmRoot )
{
	if ( !pDmRoot )
		return false;

	CDmAttribute *pConversionErrorsAttr = pDmRoot->GetAttribute( "conversionErrors", AT_STRING_ARRAY );
	if ( !pConversionErrorsAttr )
		return false;

	return ( CDmrStringArrayConst( pConversionErrorsAttr ).Count() > 0 );
}


//-----------------------------------------------------------------------------
// Feed the CDmElement returned by ReadFBX to see if there were non-fatal conversion errors which the user should be informed about, they are stored in pConversionErrors, if no errors, pConversionErrors is not touched
//-----------------------------------------------------------------------------
void CDmFbxSerializer::GetConversionErrors( CDmElement *pDmRoot, CUtlVector< CUtlString > *pConversionErrors )
{
	if ( !pDmRoot || !pConversionErrors || !HasConversionErrors( pDmRoot ) )
		return;

	CDmAttribute *pConversionErrorsAttr = pDmRoot->GetAttribute( "conversionErrors", AT_STRING_ARRAY );
	if ( !pConversionErrorsAttr )
		return;

	CDmrStringArrayConst conversionErrors( pConversionErrorsAttr );

	for ( int i = 0; i < conversionErrors.Count(); ++i )
	{
		pConversionErrors->AddToTail( conversionErrors[i] );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
FbxScene *CDmFbxSerializer::LoadFbxScene( FbxTime::EMode &eFbxTimeMode, const char *pszFilename )
{
	FbxManager *pFbxManager = GetFbxManager();
	if ( !pFbxManager )
		return NULL;

	if ( Verbose1() )
	{
		Msg( "Reading FBX: %s\n", pszFilename );
	}

	// Get the file version number generate by the FBX SDK.
	int nSDKMajor = 0;
	int nSDKMinor = 0;
	int nSDKRevision = 0;
	FbxManager::GetFileFormatVersion( nSDKMajor, nSDKMinor, nSDKRevision );

	if ( Verbose2() )
	{
		Msg( "FBX file version %d.%d.%d - SDK\n", nSDKMajor, nSDKMinor, nSDKRevision );
	}

	FbxIOSettings *pFbxIOSettings = FbxIOSettings::Create( pFbxManager, IOSROOT );
	FbxImporter *pFbxImporter = FbxImporter::Create( pFbxManager, "" );
	pFbxImporter->ParseForGlobalSettings( true );

	// Initialize the importer by providing a filename.
	const bool bImportStatus = pFbxImporter->Initialize( pszFilename, -1, pFbxIOSettings );

	int nFileMajor = 0;
	int nFileMinor = 0;
	int nFileRevision = 0;
	pFbxImporter->GetFileVersion( nFileMajor, nFileMinor, nFileRevision );

	if ( !bImportStatus )
	{
		Warning( "Warning! Couldn't import specified file as FBX \"%s\": %s\n",
			pszFilename, pFbxImporter->GetStatus().GetErrorString() );

		if ( pFbxImporter->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion )
		{
			Warning( "Warning! FBX file version mismatch SDK %d.%d.%d vs File %d.%d.%d\n",
				nSDKMajor, nSDKMinor, nSDKRevision,
				nFileMajor, nFileMinor, nFileRevision );
		}

		return NULL;
	}

	if ( pFbxImporter->IsFBX() )
	{
		if ( Verbose2() )
		{
			Msg( "FBX file version %d.%d.%d - %s\n", nFileMajor, nFileMinor, nFileRevision, pszFilename );

			// From this point, it is possible to access animation stack information without
			// the expense of loading the entire file.

			Msg( "Animation Stack Information\n");

			const int nAnimStackCount = pFbxImporter->GetAnimStackCount();

			Msg( "    Number of Animation Stacks: %d\n", nAnimStackCount );
			Msg( "    Current Animation Stack: \"%s\"\n", pFbxImporter->GetActiveAnimStackName().Buffer() );
			Msg( "\n" );

			for ( int i = 0; i < nAnimStackCount; ++i )
			{
				FbxTakeInfo *pFbxTakeInfo = pFbxImporter->GetTakeInfo( i );

				Msg( "    Animation Stack %d\n", i );
				Msg( "         Name: \"%s\"\n", pFbxTakeInfo->mName.Buffer() );
				Msg( "         Description: \"%s\"\n", pFbxTakeInfo->mDescription.Buffer() );

				// Change the value of the import name if the animation stack should be imported 
				// under a different name.
				Msg( "         Import Name: \"%s\"\n", pFbxTakeInfo->mImportName.Buffer() );

				// Set the value of the import state to false if the animation stack should be not
				// be imported. 
				Msg( "         Import State: %s\n", pFbxTakeInfo->mSelect ? "true" : "false" );
				Msg( "\n");
			}
		}

		// Set the import states. By default, the import states are always set to 
		// true. The code below shows how to change these states.
		pFbxIOSettings->SetBoolProp( IMP_FBX_MATERIAL,        true );
		pFbxIOSettings->SetBoolProp( IMP_FBX_TEXTURE,         true );
		pFbxIOSettings->SetBoolProp( IMP_FBX_LINK,            true );
		pFbxIOSettings->SetBoolProp( IMP_FBX_SHAPE,           true );
		pFbxIOSettings->SetBoolProp( IMP_FBX_GOBO,            true );
		pFbxIOSettings->SetBoolProp( IMP_FBX_ANIMATION,       true );
		pFbxIOSettings->SetBoolProp( IMP_FBX_GLOBAL_SETTINGS, true );
	}

	FbxScene *pFbxScene = FbxScene::Create( pFbxManager, "" );

	if ( pFbxScene )
	{
		// Import the scene.
		bool bStatus = pFbxImporter->Import( pFbxScene );

		if ( bStatus == false && pFbxImporter->GetStatus().GetCode() == FbxStatus::ePasswordError )
		{
			Warning( "Warning! Password protected FBX files unsupported\n" );

			pFbxScene->Destroy();
			pFbxScene = NULL;

			/* TODO: Handle password protected FBX files...

			Msg( "Please enter password: ");

			char szPassword[1024];
			szPassword[0] = '\0';

			FBXSDK_CRT_SECURE_NO_WARNING_BEGIN
				scanf( "%s", szPassword );
			FBXSDK_CRT_SECURE_NO_WARNING_END

			FbxString lString(szPassword);

			pFbxIOSettings->SetStringProp( IMP_FBX_PASSWORD,      lString );
			pFbxIOSettings->SetBoolProp( IMP_FBX_PASSWORD_ENABLE, true );

			bStatus = pFbxImporter->Import( pFbxScene );

			if ( bStatus == false && pFbxImporter->GetLastErrorID() == FbxIOBase::ePasswordError )
			{
				Msg( "\nPassword is wrong, import aborted.\n" );
				pFbxScene->Destroy();
				pFbxScene = NULL;
			}
			*/
		}

		eFbxTimeMode = FbxTime::eFrames30;

		if ( !pFbxImporter->GetFrameRate( eFbxTimeMode ) )
		{
			eFbxTimeMode = FbxTime::eFrames30;
		}

		// Destroy the importer.
		pFbxImporter->Destroy();
	}
	else
	{
		Warning( "Warning! Couldn't create FbxScene\n" );
	}

	return pFbxScene;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::LoadModelAndSkeleton_R(
	FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, CDmeDag *pDmeDagParent, FbxNode *pFbxNode, bool bAnimation, int nDepth ) const
{
	FbxString sIndent;
	for ( int i = 0; i < nDepth; ++i )
	{
		sIndent += "  ";
	}

	CDmeDag *pDmeDag = NULL;
	const char *pszFbxType = NULL;
	const char *pszDmeType = NULL;

	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();
	if ( pFbxNodeAttribute )
	{
		const FbxNodeAttribute::EType nAttributeType = pFbxNodeAttribute->GetAttributeType();

		switch ( nAttributeType )
		{
		case FbxNodeAttribute::eNull:
			pszFbxType = "Transform";
			if ( pFbxNode->GetDstObjectCount( FbxCluster::ClassId ) > 0 )
			{
				pDmeDag = FbxNodeToDmeDag( pDmeDagParent, pFbxNode, "DmeJoint" );
			}
			else
			{
				pDmeDag = FbxNodeToDmeDag( pDmeDagParent, pFbxNode, "DmeDag" );
			}
			break;
		case FbxNodeAttribute::eSkeleton:
			pszFbxType = "Skeleton";
			pDmeDag = FbxNodeToDmeDag( pDmeDagParent, pFbxNode, "DmeJoint" );
			break;
		case FbxNodeAttribute::eMesh:
		{
			pszFbxType = "Mesh";
			pszDmeType = "DmeMesh";
			FbxMatrix mScale;
			pDmeDag = FbxNodeToDmeDag( pDmeDagParent, pFbxNode, "DmeDag", &mScale );
			if ( !bAnimation )
			{
				FbxShapeToDmeMesh( pDmeDag, pFbxNode, mScale );
			}
		}
			break;
		default:
			Warning( "Warning! Ignoring Unsupported FBX Node Attribute Type: %s.%s(%s)\n",
				pFbxNode->GetName(), pFbxNodeAttribute->GetName(), pFbxNodeAttribute->GetTypeName() );
			break;
		}
	}

	if ( Verbose2() && pDmeDag && pszFbxType )
	{
		Msg( "%s + %-8s %s\n",
			sIndent.Buffer(), pszDmeType ? pszDmeType : pDmeDag->GetTypeString(), pDmeDag->GetName() );
	}

	if ( pDmeDag )
	{
		pDmeModel->AddJoint( pDmeDag );
		fbxToDmxMap.Insert( pFbxNode, pDmeDag );

		for ( int i = 0; i < pFbxNode->GetChildCount(); ++i )
		{
			LoadModelAndSkeleton_R( fbxToDmxMap, pDmeModel, pDmeDag, pFbxNode->GetChild( i ), bAnimation, nDepth + 1 );
		}
	}
}


//-----------------------------------------------------------------------------
// Converts an FbxMatrix to a Valve matrix3x4_t (transpose)
//-----------------------------------------------------------------------------
inline void MatrixFbxToValve( matrix3x4_t &mValve, const FbxMatrix &mFbx )
{
	mValve[0][0] = mFbx[0][0];	mValve[0][1] = mFbx[1][0];	mValve[0][2] = mFbx[2][0];	mValve[0][3] = mFbx[3][0];
	mValve[1][0] = mFbx[0][1];	mValve[1][1] = mFbx[1][1];	mValve[1][2] = mFbx[2][1];	mValve[1][3] = mFbx[3][1];
	mValve[2][0] = mFbx[0][2];	mValve[2][1] = mFbx[1][2];	mValve[2][2] = mFbx[2][2];	mValve[2][3] = mFbx[3][2];
}

//-----------------------------------------------------------------------------
// Converts a Valve matrix3x4_t to an FbxMatrix (transpose)
//-----------------------------------------------------------------------------
inline void MatrixValveToFbx( FbxMatrix &mFbx, const matrix3x4_t &mValve )
{
	mFbx[0][0] = mValve[0][0];	mFbx[0][1] = mValve[1][0];	mFbx[0][2] = mValve[2][0]; mFbx[3][0] = 0.0;
	mFbx[1][0] = mValve[0][1];	mFbx[1][1] = mValve[1][1];	mFbx[1][2] = mValve[2][1]; mFbx[3][1] = 0.0;
	mFbx[2][0] = mValve[0][2];	mFbx[2][1] = mValve[1][2];	mFbx[2][2] = mValve[2][2]; mFbx[3][2] = 0.0;
	mFbx[3][0] = mValve[0][3];	mFbx[3][1] = mValve[1][3];	mFbx[3][2] = mValve[2][3]; mFbx[3][3] = 1.0;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeDag *CDmFbxSerializer::FbxNodeToDmeDag( CDmeDag *pDmeDagParent, FbxNode *pFbxNode, const char *pszDmeType, FbxMatrix *pmOutScale /* = NULL */ ) const
{
	const bool bRoot = pDmeDagParent->IsA( CDmeModel::GetStaticTypeSymbol() );

	const FbxAMatrix &mFbxWorld = pFbxNode->EvaluateGlobalTransform();

	const FbxVector4 vFbxTranslate = mFbxWorld.GetT();
	const FbxQuaternion qFbxRotate = mFbxWorld.GetQ();

	Assert( vFbxTranslate[3] == 0.0 || vFbxTranslate[3] == 1.0 );

	CUtlString sName;
	GetName( sName, pFbxNode );

	Vector vDmeTranslate( vFbxTranslate[0], vFbxTranslate[1], vFbxTranslate[2] );
	Quaternion qDmeRotate( qFbxRotate[0], qFbxRotate[1], qFbxRotate[2], qFbxRotate[3] );

	DmElementHandle_t hElement = g_pDataModel->CreateElement( pszDmeType, sName.String(), pDmeDagParent->GetFileId() );
	CDmeDag *pDmeDag = CastElement< CDmeDag >( g_pDataModel->GetElement( hElement ) );

	CDmeTransform *pDmeTransform = pDmeDag->GetTransform();
	pDmeTransform->SetName( pDmeDag->GetName() );

	if ( !bRoot )
	{
		matrix3x4_t mDmeParentWorld;
		pDmeDagParent->GetAbsTransform( mDmeParentWorld );
		matrix3x4_t mDmeParentWorldInverse;
		MatrixInvert( mDmeParentWorld, mDmeParentWorldInverse );
		matrix3x4_t mDmeWorld;
		AngleMatrix( RadianEuler( qDmeRotate ), vDmeTranslate, mDmeWorld );
		matrix3x4_t mDmeLocal;
		MatrixMultiply( mDmeParentWorldInverse, mDmeWorld, mDmeLocal );
		MatrixAngles( mDmeLocal, qDmeRotate, vDmeTranslate );
	}
	else
	{
		CDmAttribute *pDmeRootNodeAttr = pDmeDag->AddAttribute( "__rootNode", AT_BOOL );
		pDmeRootNodeAttr->AddFlag( FATTRIB_DONTSAVE );
		pDmeRootNodeAttr->SetValue( bRoot );
	}

	pDmeTransform->SetOrientation( qDmeRotate );
	pDmeTransform->SetPosition( vDmeTranslate );

	pDmeDagParent->AddChild( pDmeDag );

	if ( pmOutScale )
	{
		FbxMatrix mfWf = mFbxWorld;
		FbxMatrix mfWv;

		{
			matrix3x4_t mvWv;								// mvWv = World Matrix3x4 Valve
			pDmeDag->GetAbsTransform( mvWv );
			MatrixValveToFbx( mfWv, mvWv );
		}

		const FbxMatrix mfWv_ = mfWv.Inverse();
		*pmOutScale = mfWv_ * mfWf;
	}

	return pDmeDag;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool AddPositionData( CDmeVertexData *pDmeVertexData, const CUtlVector< int > &nIndices, const FbxMesh *pFbxMesh, const FbxMatrix &mScale )
{
	if ( !pFbxMesh || nIndices.Count() <= 0 )
		return false;

	const FbxVector4 *pvFbxData = pFbxMesh->GetControlPoints();
	const int nDataCount = pFbxMesh->GetControlPointsCount();

	CUtlVector< Vector > dmxData;
	dmxData.SetCount( nDataCount );

	for ( int i = 0; i < dmxData.Count(); ++i )
	{
		Vector &vDmxData = dmxData[i];
		const FbxVector4 vFbxData = mScale.MultNormalize( pvFbxData[i] );
		vDmxData.x = vFbxData[0];
		vDmxData.y = vFbxData[1];
		vDmxData.z = vFbxData[2];

		Assert( vFbxData[3] == 0 || vFbxData[3] == 1 );
	}

	const FieldIndex_t nFieldIndex = pDmeVertexData->CreateField( CDmeVertexDataBase::FIELD_POSITION );
	pDmeVertexData->AddVertexData( nFieldIndex, dmxData.Count() );
	pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxData.Count(), AT_VECTOR3, dmxData.Base() );
	pDmeVertexData->AddVertexIndices( nIndices.Count() );
	pDmeVertexData->SetVertexIndices( nFieldIndex, 0, nIndices.Count(), nIndices.Base() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void AddUVData( CDmeVertexData *pDmeVertexData, const CUtlVector< int > &nIndices, const FbxGeometryElementUV *pFbxElement, int nSemanticIndex )
{
	if ( !pFbxElement || nIndices.Count() <= 0 )
		return;

	const FbxLayerElementArrayTemplate< FbxVector2 > &fbxData = pFbxElement->GetDirectArray();
	const int nDataCount = fbxData.GetCount();

	CUtlVector< Vector2D > dmxData;
	dmxData.SetCount( nDataCount );

	for ( int i = 0; i < dmxData.Count(); ++i )
	{
		Vector2D &vDmxData = dmxData[i];
		const FbxVector2 vFbxData = fbxData[i];
		vDmxData.x = vFbxData[0];
		vDmxData.y = vFbxData[1];
	}

	FieldIndex_t nFieldIndex = -1;

	if ( nSemanticIndex == 0 )
	{
		nFieldIndex = pDmeVertexData->CreateField( CDmeVertexDataBase::FIELD_TEXCOORD );
	}
#ifdef SOURCE2
	else if ( nSemanticIndex == 1 )
	{
		nFieldIndex = pDmeVertexData->CreateField( CDmeVertexDataBase::FIELD_TEXCOORD2 );
	}
#endif
	else
	{
		// No FIELD_TEXCOORD3, no method to get the standard field name without the semantic index
		// so magically we know it will be "texcoord$#" where # is the semantic index
		nFieldIndex = pDmeVertexData->CreateField< Vector2D >( CFmtStr( "texcoord$%d", nSemanticIndex ).Get() );
	}
	pDmeVertexData->AddVertexData( nFieldIndex, dmxData.Count() );
	pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxData.Count(), AT_VECTOR2, dmxData.Base() );
	pDmeVertexData->SetVertexIndices( nFieldIndex, 0, nIndices.Count(), nIndices.Base() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void AddNormalData( CDmeVertexData *pDmeVertexData, const CUtlVector< int > &nIndices, const FbxGeometryElementNormal *pFbxElement, const FbxMatrix &mScale )
{
	if ( !pFbxElement || nIndices.Count() <= 0 )
		return;

	// Normals being FbxVector4's are a little weird but just using x, y, z is fine.
	// There seems to be breakage when loading an FBX 2014 file in FBX 2013 SDK because they
	// added a NormalsW layer which isn't actually put into the W component but this gets the
	// right x, y, z values regardless if w is just ignored

	const FbxLayerElementArrayTemplate< FbxVector4 > &fbxData = pFbxElement->GetDirectArray();
	const int nDataCount = fbxData.GetCount();

	CUtlVector< Vector > dmxData;
	dmxData.SetCount( nDataCount );

	if ( mScale == FbxMatrix() )
	{
		// If the scale is the identity matrix

		for ( int i = 0; i < dmxData.Count(); ++i )
		{
			Vector &vDmxData = dmxData[i];
			const FbxVector4 vFbxData = fbxData[i];
			vDmxData.x = vFbxData[0];
			vDmxData.y = vFbxData[1];
			vDmxData.z = vFbxData[2];
		}
	}
	else
	{
		const FbxMatrix mInvTranspose = mScale.Inverse().Transpose();

		for ( int i = 0; i < dmxData.Count(); ++i )
		{
			Vector &vDmxData = dmxData[i];
			FbxVector4 vFbxData = mInvTranspose.MultNormalize( fbxData[i] );
			vFbxData.Normalize();
			vDmxData.x = vFbxData[0];
			vDmxData.y = vFbxData[1];
			vDmxData.z = vFbxData[2];
		}
	}

	const FieldIndex_t nFieldIndex = pDmeVertexData->CreateField( CDmeVertexDataBase::FIELD_NORMAL );
	pDmeVertexData->AddVertexData( nFieldIndex, dmxData.Count() );
	pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxData.Count(), AT_VECTOR3, dmxData.Base() );
	pDmeVertexData->SetVertexIndices( nFieldIndex, 0, nIndices.Count(), nIndices.Base() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::AddColorData( CDmeVertexData *pDmeVertexData, const CUtlVector< int > &nIndices, const FbxGeometryElementVertexColor *pFbxElement ) const
{
	if ( !pFbxElement || nIndices.Count() <= 0 )
		return;

	const FbxLayerElementArrayTemplate< FbxColor > &fbxData = pFbxElement->GetDirectArray();
	const int nDataCount = fbxData.GetCount();

	if ( nDataCount <= 0 )
		return;

	const char *pszName = pFbxElement->GetName();
	const char *pszChannelName = pszName;

	bool bUseChannelName = false;
	DmAttributeType_t dmAttributeType = AT_COLOR;

	if ( StringHasPrefix( pszName, "export_" ) )
	{
		pszChannelName = StringAfterPrefix( pszName, "export_" );
		if ( V_strlen( pszChannelName ) > 0 )
		{
			bUseChannelName = true;

			// Currently "foliageanimation" is a special case which gets converted to a Vector3
			if ( !V_stricmp( pszChannelName, "foliageanimation" ) )
			{
				dmAttributeType = AT_VECTOR3;
				pszChannelName = "foliageanimation";	// Make it lowercase for certain
			}
		}
		else
		{
			pszChannelName = pszName;
		}
	}

	FieldIndex_t nFieldIndex = -1;

	if ( bUseChannelName )
	{
		nFieldIndex = pDmeVertexData->CreateField( pszChannelName, ValueTypeToArrayType( dmAttributeType ) );
	}
	else
	{
		CFmtStr sFieldName;
		int nSemanticIndex = 0;

		for ( ;; )
		{
			sFieldName.sprintf( "color$%d", nSemanticIndex );
			FieldIndex_t nTmpField = pDmeVertexData->FindFieldIndex( sFieldName.Get() );
			if ( nTmpField < 0 )
				break;

			++nSemanticIndex;
		}

		nFieldIndex = pDmeVertexData->CreateField( sFieldName.Get(), ValueTypeToArrayType( dmAttributeType ) );
	}

	if ( nFieldIndex < 0 )
	{
		AddConversionError( pDmeVertexData->GetFileId(), CFmtStr( "Couldn't convert color field: %s\n", pszName ).Get() );
		return;
	}

	if ( dmAttributeType == AT_VECTOR3 )
	{
		CUtlVector< Vector > dmxData;
		dmxData.SetCount( nDataCount );

		for ( int i = 0; i < dmxData.Count(); ++i )
		{
			Vector &vDmxData = dmxData[i];
			const FbxColor vFbxData = fbxData[i];
			vDmxData[0] = vFbxData[0];
			vDmxData[1] = vFbxData[1];
			vDmxData[2] = vFbxData[2];
			// Alpha gets discarded when converting to Vector3, all FBX color channels are RGBA
		}

		pDmeVertexData->AddVertexData( nFieldIndex, dmxData.Count() );
		pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxData.Count(), dmAttributeType, dmxData.Base() );
	}
	else
	{
		Assert( dmAttributeType == AT_COLOR );

		CUtlVector< Color > dmxData;
		dmxData.SetCount( nDataCount );

		for ( int i = 0; i < dmxData.Count(); ++i )
		{
			Color &vDmxData = dmxData[i];
			const FbxColor vFbxData = fbxData[i];
			vDmxData.SetColor(
				clamp( static_cast< uint8 >( floor( vFbxData[0] * 255.0 ) ), 0, 255 ),
				clamp( static_cast< uint8 >( floor( vFbxData[1] * 255.0 ) ), 0, 255 ),
				clamp( static_cast< uint8 >( floor( vFbxData[2] * 255.0 ) ), 0, 255 ),
				clamp( static_cast< uint8 >( floor( vFbxData[3] * 255.0 ) ), 0, 255 ) );
		}

		pDmeVertexData->AddVertexData( nFieldIndex, dmxData.Count() );
		pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxData.Count(), dmAttributeType, dmxData.Base() );
	}

	pDmeVertexData->SetVertexIndices( nFieldIndex, 0, nIndices.Count(), nIndices.Base() );
}


//-----------------------------------------------------------------------------
// UV Color data is a special case for 3DSMax
//-----------------------------------------------------------------------------
void CDmFbxSerializer::AddUVColorData( CDmeVertexData *pDmeVertexData, const UVColorChannelData_t &uvColorData ) const
{
	// Normally UVColor data is exported as a VECTOR3 data in DMX, set this to true to also export as a color map
	const bool bExportColor = false;

	for ( int i = 0; i < ARRAYSIZE( uvColorData.m_uvChannelData ); ++i )
	{
		if ( !uvColorData.m_uvChannelData[i].m_pFbxElementUV || uvColorData.m_uvChannelData[i].m_nIndices.IsEmpty() )
			return;
	}

	CUtlVector< Color > dmxColorData;
	CUtlVector< Vector > dmxData;
	CUtlVector< int > indices;

	for ( int i = 0; i < ARRAYSIZE( uvColorData.m_uvChannelData ); ++i )
	{
		const CUtlVector< int > &srcIndices = uvColorData.m_uvChannelData[i].m_nIndices;

		if ( indices.IsEmpty() )
		{
			const int nIndexCount = srcIndices.Count();

			indices.SetCount( nIndexCount );
			dmxColorData.SetCount( nIndexCount );
			dmxData.SetCount( nIndexCount );

			for ( int j = 0; j < nIndexCount; ++j )
			{
				indices[j] = j;
			}

			if ( bExportColor )
			{
				for ( int j = 0; j < nIndexCount; ++j )
				{
					dmxColorData[j] = Color( 0, 0, 0, 255 );
				}
			}
		}
		else
		{
			if ( indices.Count() != srcIndices.Count() )
			{
				Warning( "Warning! Fbx 3DS Max index mismatch for channel \"%s\", got %d expected %d\n", uvColorData.m_uvChannelData[i].m_pFbxElementUV->GetName(), srcIndices.Count(), indices.Count() );
				return;
			}
		}
	}

	for ( int i = 0; i < ARRAYSIZE( uvColorData.m_uvChannelData ); ++i )
	{
		const FbxLayerElementArrayTemplate< FbxVector2 > &fbxData = uvColorData.m_uvChannelData[i].m_pFbxElementUV->GetDirectArray();
		const int nDataCount = fbxData.GetCount();

		if ( nDataCount <= 0 )
			return;

		const CUtlVector< int > &fbxIndices = uvColorData.m_uvChannelData[i].m_nIndices;

		for ( int j = 0; j < indices.Count(); ++j )
		{
			const int nDmxDataIndex = indices[j];	Assert( nDmxDataIndex == j );
			const int nFbxDataIndex = fbxIndices[nDmxDataIndex];

			const FbxVector2 &vFbxData = fbxData[nFbxDataIndex];

			Vector &vDmxData = dmxData[nDmxDataIndex];
			vDmxData[i] = vFbxData[0];	// Always use value 0

			if ( bExportColor )
			{
				Color &vDmxColorData = dmxColorData[nDmxDataIndex];
				vDmxColorData[i] = clamp( static_cast< uint8 >( floor( vFbxData[0] * 255.0 ) ), 0, 255 );	// Always use value 0
			}
		}
	}

	{
		const DmAttributeType_t dmAttributeType = AT_VECTOR3;
		const FieldIndex_t nFieldIndex = pDmeVertexData->CreateField( uvColorData.m_sChannelName.Get(), ValueTypeToArrayType( dmAttributeType ) );

		pDmeVertexData->AddVertexData( nFieldIndex, dmxData.Count() );
		pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxData.Count(), dmAttributeType, dmxData.Base() );
		pDmeVertexData->SetVertexIndices( nFieldIndex, 0, indices.Count(), indices.Base() );

		pDmeVertexData->Resolve();
	}

	if ( bExportColor )
	{
		CFmtStr sFieldName;
		int nSemanticIndex = 0;

		for ( ;; )
		{
			sFieldName.sprintf( "color$%d", nSemanticIndex );
			FieldIndex_t nTmpField = pDmeVertexData->FindFieldIndex( sFieldName.Get() );
			if ( nTmpField < 0 )
				break;

			++nSemanticIndex;
		}

		const DmAttributeType_t dmAttributeType = AT_COLOR;
		const FieldIndex_t nFieldIndex = pDmeVertexData->CreateField( sFieldName.Get(), ValueTypeToArrayType( dmAttributeType ) );

		pDmeVertexData->AddVertexData( nFieldIndex, dmxColorData.Count() );
		pDmeVertexData->SetVertexData( nFieldIndex, 0, dmxColorData.Count(), dmAttributeType, dmxColorData.Base() );
		pDmeVertexData->SetVertexIndices( nFieldIndex, 0, indices.Count(), indices.Base() );

		pDmeVertexData->Resolve();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < FbxLayerElement::EMappingMode M, FbxLayerElement::EReferenceMode R >
void HandleUV( CUtlVector< int > &uvIndices, FbxMesh *pFbxMesh, FbxGeometryElementUV *peUV, int nControlPointIndex, int nPolygonVertexIndex )
{
	// Do Nothing
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleUV< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eDirect >(
	CUtlVector< int > &uvIndices, FbxMesh *pFbxMesh, FbxGeometryElementUV *peUV, int nControlPointIndex, int nPolygonVertexIndex )
{
	uvIndices.AddToTail( nControlPointIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleUV< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eIndexToDirect >(
	CUtlVector< int > &uvIndices, FbxMesh *pFbxMesh, FbxGeometryElementUV *peUV, int nControlPointIndex, int nPolygonVertexIndex )
{
	const int nUVId = peUV->GetIndexArray().GetAt( nControlPointIndex );
	uvIndices.AddToTail( nUVId );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleUV< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eDirect >(
	CUtlVector< int > &uvIndices, FbxMesh *pFbxMesh, FbxGeometryElementUV *peUV, int nControlPointIndex, int nPolygonVertexIndex )
{
	uvIndices.AddToTail( nPolygonVertexIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleUV< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eIndexToDirect >(
	CUtlVector< int > &uvIndices, FbxMesh *pFbxMesh, FbxGeometryElementUV *peUV, int nControlPointIndex, int nPolygonVertexIndex )
{
	const int nUVId = peUV->GetIndexArray().GetAt( nPolygonVertexIndex );
	uvIndices.AddToTail( nUVId );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmFbxSerializer::HandleUVFunc_t GetHandleUVFunc( FbxGeometryElementUV *pFbxElementUV )
{
	if ( pFbxElementUV )
	{
		const FbxLayerElement::EMappingMode nMappingMode = pFbxElementUV->GetMappingMode();
		const FbxLayerElement::EReferenceMode nReferenceMode = pFbxElementUV->GetReferenceMode();

		if ( nMappingMode == FbxGeometryElement::eByControlPoint && nReferenceMode == FbxGeometryElement::eDirect )
		{
			return HandleUV< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByControlPoint && nReferenceMode == FbxGeometryElement::eIndexToDirect )
		{
			return HandleUV< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eIndexToDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByPolygonVertex && nReferenceMode == FbxGeometryElement::eDirect )
		{
			return HandleUV< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByPolygonVertex && nReferenceMode == FbxGeometryElement::eIndexToDirect )
		{
			return HandleUV < FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eIndexToDirect > ;
		}
		else
		{
			Warning( "Warning! Unsupported FBX UV Mapping/Reference Mode %d/%d\n", nMappingMode, nReferenceMode );
		}
	}

	return HandleUV< FbxGeometryElement::eNone, FbxGeometryElement::eDirect >;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < FbxLayerElement::EMappingMode M, FbxLayerElement::EReferenceMode R >
void HandleNormal(
	CUtlVector< int > &nNormalIndices, FbxMesh *pFbxMesh, FbxGeometryElementNormal *pFbxElementNormal, int nControlPointIndex, int nPolygonVertexIndex )
{
	// Do Nothing
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleNormal< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eDirect >(
	CUtlVector< int > &nNormalIndices, FbxMesh *pFbxMesh, FbxGeometryElementNormal *pFbxElementNormal, int nControlPointIndex, int nPolygonVertexIndex )
{
	nNormalIndices.AddToTail( nControlPointIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleNormal< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eDirect >(
	CUtlVector< int > &nNormalIndices, FbxMesh *pFbxMesh, FbxGeometryElementNormal *pFbxElementNormal, int nControlPointIndex, int nPolygonVertexIndex )
{
	nNormalIndices.AddToTail( nPolygonVertexIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleNormal< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eIndexToDirect >(
	CUtlVector< int > &nNormalIndices, FbxMesh *pFbxMesh, FbxGeometryElementNormal *pFbxElementNormal, int nControlPointIndex, int nPolygonVertexIndex )
{
	const int nNormalIndex = pFbxElementNormal->GetIndexArray().GetAt( nPolygonVertexIndex );
	nNormalIndices.AddToTail( nNormalIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
typedef void (*HandleNormalFunc_t)( CUtlVector< int > &, FbxMesh *, FbxGeometryElementNormal *, int, int );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HandleNormalFunc_t GetHandleNormalFunc( FbxGeometryElementNormal *pFbxElementNormal )
{
	if ( pFbxElementNormal )
	{
		const FbxLayerElement::EMappingMode nMappingMode = pFbxElementNormal->GetMappingMode();
		const FbxLayerElement::EReferenceMode nReferenceMode = pFbxElementNormal->GetReferenceMode();

		if ( nMappingMode == FbxGeometryElement::eByControlPoint && nReferenceMode == FbxGeometryElement::eDirect )
		{
			return HandleNormal< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByPolygonVertex && nReferenceMode == FbxGeometryElement::eDirect )
		{
			return HandleNormal< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByPolygonVertex && nReferenceMode == FbxGeometryElement::eIndexToDirect )
		{
			return HandleNormal< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eIndexToDirect >;
		}
		else
		{
			Warning( "Warning! Unsupported FBX Normal Mapping/Reference Mode %d/%d\n", nMappingMode, nReferenceMode );
		}
	}

	return HandleNormal< FbxGeometryElement::eNone, FbxGeometryElement::eDirect >;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < FbxLayerElement::EMappingMode M, FbxLayerElement::EReferenceMode R >
void HandleVertexColor(
	CUtlVector< int > &vertexColorIndices, FbxMesh *pFbxMesh, FbxGeometryElementVertexColor *peVertexColor, int nControlPointIndex, int nPolygonVertexIndex )
{
	// Do Nothing
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleVertexColor< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eDirect >(
	CUtlVector< int > &vertexColorIndices, FbxMesh *pFbxMesh, FbxGeometryElementVertexColor *peVertexColor, int nControlPointIndex, int nPolygonVertexIndex )
{
	vertexColorIndices.AddToTail( nControlPointIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleVertexColor< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eIndexToDirect >(
	CUtlVector< int > &vertexColorIndices, FbxMesh *pFbxMesh, FbxGeometryElementVertexColor *peVertexColor, int nControlPointIndex, int nPolygonVertexIndex )
{
	const int nVertexColorId = peVertexColor->GetIndexArray().GetAt( nControlPointIndex );
	vertexColorIndices.AddToTail( nVertexColorId );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleVertexColor< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eDirect >(
	CUtlVector< int > &vertexColorIndices, FbxMesh *pFbxMesh, FbxGeometryElementVertexColor *peVertexColor, int nControlPointIndex, int nPolygonVertexIndex )
{
	vertexColorIndices.AddToTail( nPolygonVertexIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template <>
void HandleVertexColor< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eIndexToDirect >(
	CUtlVector< int > &vertexColorIndices, FbxMesh *pFbxMesh, FbxGeometryElementVertexColor *peVertexColor, int nControlPointIndex, int nPolygonVertexIndex )
{
	const int nVertexColorId = peVertexColor->GetIndexArray().GetAt( nPolygonVertexIndex );
	vertexColorIndices.AddToTail( nVertexColorId );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
typedef void (*HandleVertexColorFunc_t)( CUtlVector< int > &, FbxMesh *, FbxGeometryElementVertexColor *, int, int );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HandleVertexColorFunc_t GetHandleVertexColorFunc( FbxGeometryElementVertexColor *pFbxElementVertexColor )
{
	if ( pFbxElementVertexColor )
	{
		const FbxLayerElement::EMappingMode nMappingMode = pFbxElementVertexColor->GetMappingMode();
		const FbxLayerElement::EReferenceMode nReferenceMode = pFbxElementVertexColor->GetReferenceMode();

		if ( nMappingMode == FbxGeometryElement::eByControlPoint && nReferenceMode == FbxGeometryElement::eDirect )
		{
			return HandleVertexColor< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByControlPoint && nReferenceMode == FbxGeometryElement::eIndexToDirect )
		{
			return HandleVertexColor< FbxGeometryElement::eByControlPoint, FbxGeometryElement::eIndexToDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByPolygonVertex && nReferenceMode == FbxGeometryElement::eDirect )
		{
			return HandleVertexColor< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eDirect >;
		}
		else if ( nMappingMode == FbxGeometryElement::eByPolygonVertex && nReferenceMode == FbxGeometryElement::eIndexToDirect )
		{
			return HandleVertexColor< FbxGeometryElement::eByPolygonVertex, FbxGeometryElement::eIndexToDirect >;
		}
		else
		{
			Warning( "Warning! Unsupported FBX VertexColor Mapping/Reference Mode %d/%d\n", nMappingMode, nReferenceMode );
		}
	}

	return HandleVertexColor< FbxGeometryElement::eNone, FbxGeometryElement::eDirect >;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
struct ColorChannelData_t
{
public:
	ColorChannelData_t()
	: m_pFbxElementVertexColor( NULL )
	, m_pFunc( NULL )
	{
	}

	FbxGeometryElementVertexColor *m_pFbxElementVertexColor;
	HandleVertexColorFunc_t m_pFunc;
	CUtlVector< int > m_nIndices;
};


//-----------------------------------------------------------------------------
//
// This converts an FbxMesh into a DmeMesh where the vertex data indices for
// the DmeMesh will be full expanded to be per polygon per vertex values
//
//-----------------------------------------------------------------------------
CDmeMesh *CDmFbxSerializer::FbxShapeToDmeMesh( CDmeDag *pDmeDag, FbxNode *pFbxNode, const FbxMatrix &mScale ) const
{
	bool bReverse = false;

	{
		FbxVector4 vTTmp;
		FbxQuaternion qRTmp;
		FbxVector4 vShTmp;
		FbxVector4 vScTmp;

		double flSign = 1.0;	// Sign of the determinant of the scale matrix, if negative, odd number of scales and faces need reversing
		mScale.GetElements( vTTmp, qRTmp, vShTmp, vScTmp, flSign );

		bReverse = ( flSign < 0.0 );
	}

	if ( !pDmeDag || !pFbxNode )
		return NULL;

	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();
	if ( !pFbxNodeAttribute || pFbxNodeAttribute->GetAttributeType() != FbxNodeAttribute::eMesh )
		return NULL;

	FbxMesh *pFbxMesh = reinterpret_cast< FbxMesh * >( pFbxNodeAttribute );
	if ( !pFbxMesh )
		return NULL;

	CUtlString sName;
	GetName( sName, pFbxNode );

	CDmeMesh *pDmeMesh = CreateElement< CDmeMesh >( sName.String(), pDmeDag->GetFileId() );
	if ( !pDmeMesh )
	{
		Warning( "Warning! Couldn't create DmeMesh for FbxMesh \"%s\"\n", pFbxMesh->GetName() );
		return NULL;
	}

	CUtlVector< int > nPolygonToFaceSetMap;
	FbxMeshToDmeFaceSets( pDmeDag, pDmeMesh, pFbxMesh, nPolygonToFaceSetMap );

	const int nNormalCount = pFbxMesh->GetElementNormalCount();
	FbxGeometryElementNormal *pFbxElementNormal = nNormalCount > 0 ? pFbxMesh->GetElementNormal( 0 ) : NULL;
	HandleNormalFunc_t pHandleNormalFunc = GetHandleNormalFunc( pFbxElementNormal );

	CUtlVector< int > nPositionIndices;
	CUtlVector< int > nNormalIndices;

	CUtlVector< ColorChannelData_t > colorChannelDataList;

	const int nColorCount = pFbxMesh->GetElementVertexColorCount();
	for ( int nColor = 0; nColor < nColorCount; ++nColor  )
	{
		FbxGeometryElementVertexColor *pFbxElementVertexColor = pFbxMesh->GetElementVertexColor( nColor );
		if ( !pFbxElementVertexColor )
			continue;

		ColorChannelData_t &colorChannelData = colorChannelDataList[colorChannelDataList.AddToTail()];
		colorChannelData.m_pFbxElementVertexColor = pFbxElementVertexColor;
		colorChannelData.m_pFunc = GetHandleVertexColorFunc( pFbxElementVertexColor );
	}


	CUtlVector< UVColorChannelData_t > uvColorChannelDataList;

	int nUVElementCount = pFbxMesh->GetElementUVCount();

	CUtlVector< UVChannelData_t > uvChannelList;

	if ( nUVElementCount > 0 )
	{
		CUtlVector< FbxGeometryElementUV * > uvElementList;

		uvElementList.SetCount( nUVElementCount );
		for ( int nUV = 0; nUV < nUVElementCount; ++nUV )
		{
			uvElementList[nUV] = pFbxMesh->GetElementUV( nUV );
		}

		if ( nUVElementCount >= 4 )
		{
			// For 3DS Max, there seems to be a bug in how 3DS Max exports extra vertex color information, it's sticking it into UVs
			// See if there are three UV channels:
			// "export_foliageanimation_r",
			// "export_foliageanimation_g",
			// "export_foliageanimation_b"

			const char *szChannelNames[3] = { "r", "g", "b" };
			FbxGeometryElementUV *pUVs[3] = { NULL, NULL, NULL };
			int nUVFoundCount = 0;
			bool bValid = true;

			COMPILE_TIME_ASSERT( ARRAYSIZE( szChannelNames ) == ARRAYSIZE( pUVs ) );

			const char *pszPrefix = "export_foliageanimation_";
			const char *pszChannelName = "foliageanimation";

			for ( int nUV = 0; bValid && nUV < nUVElementCount; ++nUV )
			{
				FbxGeometryElementUV *pUV = pFbxMesh->GetElementUV( nUV );
				const char *pszUVName = pUV->GetName();
				if ( StringHasPrefix( pszUVName, pszPrefix ) )
				{
					const char *pszChannel = StringAfterPrefix( pszUVName, pszPrefix );

					bool bUVFound = false;

					for ( int i = 0; i < ARRAYSIZE( szChannelNames ); ++i )
					{
						if ( !V_stricmp( pszChannel, szChannelNames[i] ) )
						{
							bUVFound = true;

							if ( pUVs[i] == NULL )
							{
								pUVs[i] = pUV;
								++nUVFoundCount;
							}
							else
							{
								Warning( "Warning! Fbx 3DSMax UV encoded VertexPaint channel \"%s\", specified multiple times on mesh \"%s\"\n", pszUVName, pFbxMesh->GetName() );
							}
							break;
						}
					}

					if ( bValid && !bUVFound )
					{
						Warning( "Warning! Fbx 3DSMax UV encoded VertexPaint channel \"%s\", unexpected, expecting ending of [ r, g or b ], on mesh \"%s\"\n", pszUVName, pFbxMesh->GetName() );
					}
				}
			}

			if ( bValid && nUVFoundCount == 3 )
			{
				UVColorChannelData_t &uvEncodedColorChannelData = uvColorChannelDataList[uvColorChannelDataList.AddToTail()];

				uvEncodedColorChannelData.m_sChannelName = pszChannelName;

				for ( int i = 0; i < ARRAYSIZE( pUVs ); ++i )
				{
					uvEncodedColorChannelData.m_uvChannelData[i].m_pFbxElementUV = pUVs[i];
					uvEncodedColorChannelData.m_uvChannelData[i].m_pFunc = GetHandleUVFunc( pUVs[i] );
					uvElementList.FindAndRemove( pUVs[i] );	// Remove these funky 3ds max channels from the normal UV channels
				}
			}
			else if ( nUVFoundCount > 0 )
			{
				Warning( "Warning! Fbx 3DSMax UV encoded VertexPaint \"%s\" invalid, expected %llu maps, found %d, on mesh \"%s\"\n", pszPrefix, static_cast< uint64 >( ARRAYSIZE( szChannelNames ) ), nUVFoundCount, pFbxMesh->GetName() );
			}
		}

		// Add the non-3DsMax UV encoded color channels to the actual UV channel list
		nUVElementCount = uvElementList.Count();

		for ( int nUV = 0; nUV < nUVElementCount; ++nUV )
		{
			FbxGeometryElementUV *pFbxElementUV = pFbxMesh->GetElementUV( nUV );
			if ( !pFbxElementUV )
				continue;

			HandleUVFunc_t pHandleUVFunc = GetHandleUVFunc( pFbxElementUV );
			if ( !pHandleUVFunc )
				continue;

			UVChannelData_t &uvChannelData = uvChannelList[uvChannelList.AddToTail()];
			uvChannelData.m_pFbxElementUV = pFbxElementUV;
			uvChannelData.m_pFunc = pHandleUVFunc;
		}

		nUVElementCount = uvChannelList.Count();
	}

	const int nUVColorCount = uvColorChannelDataList.Count();

	CUtlVector< int > nFaceSetIndices;

	const int nPolygonCount = pFbxMesh->GetPolygonCount();
	int nPolygonVertexIndex = 0;

	for ( int nPolygon = 0; nPolygon < nPolygonCount; ++nPolygon )
	{
		const int nFaceSet = nPolygonToFaceSetMap[nPolygon];
		CDmeFaceSet *pDmeFaceSet = pDmeMesh->GetFaceSet( nFaceSet );

		const int nPolygonVertCount = pFbxMesh->GetPolygonSize( nPolygon );
		nFaceSetIndices.SetCount( nPolygonVertCount + 1 );

		for ( int nPolygonVert = 0; nPolygonVert < nPolygonVertCount; ++nPolygonVert )
		{
			const int nControlPointIndex = pFbxMesh->GetPolygonVertex( nPolygon, nPolygonVert );

			nFaceSetIndices[nPolygonVert] = nPositionIndices.Count();

			nPositionIndices.AddToTail( nControlPointIndex );
			for ( int nUV = 0; nUV < nUVElementCount; ++nUV  )
			{
				UVChannelData_t &uvChannelData = uvChannelList[nUV];
				( *( uvChannelData.m_pFunc ) )( uvChannelData.m_nIndices, pFbxMesh, uvChannelData.m_pFbxElementUV, nControlPointIndex, nPolygonVertexIndex );
			}
			( *pHandleNormalFunc )( nNormalIndices, pFbxMesh, pFbxElementNormal, nControlPointIndex, nPolygonVertexIndex );

			for ( int nColor = 0; nColor < nColorCount; ++nColor  )
			{
				ColorChannelData_t &colorChannelData = colorChannelDataList[nColor];
				(*colorChannelData.m_pFunc)( colorChannelData.m_nIndices, pFbxMesh, colorChannelData.m_pFbxElementVertexColor, nControlPointIndex, nPolygonVertexIndex );
			}

			for ( int nUVColor = 0; nUVColor < nUVColorCount; ++nUVColor  )
			{
				UVColorChannelData_t &uvColorChannelData = uvColorChannelDataList[nUVColor];
				for ( int nUVChannel = 0; nUVChannel < ARRAYSIZE( uvColorChannelData.m_uvChannelData ); ++nUVChannel )
				{
					( *( uvColorChannelData.m_uvChannelData[nUVChannel].m_pFunc ) )( uvColorChannelData.m_uvChannelData[nUVChannel].m_nIndices, pFbxMesh, uvColorChannelData.m_uvChannelData[nUVChannel].m_pFbxElementUV, nControlPointIndex, nPolygonVertexIndex );
				}
			}

			++nPolygonVertexIndex;
		}

		nFaceSetIndices[nPolygonVertCount] = -1;

		const int nStartIndex = pDmeFaceSet->NumIndices();

		pDmeFaceSet->AddIndices( nFaceSetIndices.Count() );

		if ( bReverse )
		{
			int nCurrentIndex = nStartIndex;
			// Add indices for this face in reverse order, leaving the -1 as the last index (face terminator)
			for ( int nReverseIndex = nFaceSetIndices.Count() - 2; nReverseIndex >= 0; --nReverseIndex )
			{
				pDmeFaceSet->SetIndex( nCurrentIndex, nFaceSetIndices[nReverseIndex] );
				++nCurrentIndex;
			}

			pDmeFaceSet->SetIndex( nCurrentIndex, nFaceSetIndices.Tail() );	// Add the -1
		}
		else
		{
			pDmeFaceSet->SetIndices( nStartIndex, nFaceSetIndices.Count(), nFaceSetIndices.Base() );
		}
	}

	CDmeVertexData *pDmeVertexData = pDmeMesh->FindOrCreateBaseState( "bind" );
	pDmeVertexData->FlipVCoordinate( true );
	pDmeMesh->SetBindBaseState( pDmeVertexData );
	pDmeMesh->SetCurrentBaseState( "bind" );

	if ( !AddPositionData( pDmeVertexData, nPositionIndices, pFbxMesh, mScale ) )
	{
		Warning( "Warning! Couldn't convert FbxMesh \"%s\"\n", pFbxMesh->GetName() );
		g_pDataModel->DestroyElement( pDmeMesh->GetHandle() );
		return NULL;
	}

	for ( int nUV = 0; nUV < nUVElementCount; ++nUV )
	{
		const UVChannelData_t &uvChannelData = uvChannelList[nUV];
		AddUVData( pDmeVertexData, uvChannelData.m_nIndices, uvChannelData.m_pFbxElementUV, nUV );
	}

	AddNormalData( pDmeVertexData, nNormalIndices, pFbxElementNormal, mScale );

	for ( int nColor = 0; nColor < nColorCount; ++nColor  )
	{
		ColorChannelData_t &colorChannelData = colorChannelDataList[nColor];
		AddColorData( pDmeVertexData, colorChannelData.m_nIndices, colorChannelData.m_pFbxElementVertexColor );
	}

	for ( int nUVColor = 0; nUVColor < uvColorChannelDataList.Count(); ++nUVColor  )
	{
		UVColorChannelData_t &uvColorChannelData = uvColorChannelDataList[nUVColor];
		AddUVColorData( pDmeVertexData, uvColorChannelData );
	}

	pDmeDag->SetShape( pDmeMesh );

	return pDmeMesh;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmFbxSerializer::FbxMeshToDmeFaceSets( CDmeDag *pDmeDag, CDmeMesh *pDmeMesh, FbxMesh *pFbxMesh, CUtlVector< int > &nPolygonToFaceSetMap ) const
{
	const int nPolygonCount = pFbxMesh->GetPolygonCount();
	const int nMaterialCount = pFbxMesh->GetNode()->GetMaterialCount();
	const int nElementMaterialCount = pFbxMesh->GetElementMaterialCount();

	FbxGeometryElementMaterial *pMatElement = NULL;

	if ( nMaterialCount > 0 && nElementMaterialCount > 0 )
	{
		pMatElement = pFbxMesh->GetElementMaterial( 0 );
	}

	CUtlString sFallbackMaterialPath;

#ifdef SOURCE2
	{
		// Generate a fallback material path based on the mesh name, only used if material cannot be located
		char szResourceName[MAX_PATH] = {};
		if ( !FixupResourceName( pDmeMesh->GetName(), RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) ) )
		{
			SetResourceFileNameExtension( pDmeMesh->GetName(), RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) );
		}
		sFallbackMaterialPath = szResourceName;
	}
#endif
	CUtlString sMaterialPath = sFallbackMaterialPath;

	// Looking for UserData on a mesh ending in .vmat
	static const char szSuffix[] = "_vmat";
	const int nSuffixCount = ARRAYSIZE( szSuffix ) - 1;
	COMPILE_TIME_ASSERT( nSuffixCount == 5 );

	typedef FbxMap< FbxString, FbxLayerElementArrayTemplate< void * > * > HoudiniMatDataMap_t;	// data to figure out material assignments from houdini
	HoudiniMatDataMap_t houdiniMatData;

	for ( int i = 0; i < pFbxMesh->GetElementUserDataCount(); ++i )
	{
		FbxGeometryElementUserData *pFbxUserData = pFbxMesh->GetElementUserData( i );;

		for ( int j = 0; j < pFbxUserData->GetDirectArrayCount(); ++j )
		{
			if ( pFbxUserData->GetDataType( j ).GetType() == eFbxInt )
			{
				const char *pszDataName = pFbxUserData->GetDataName( j );
				FbxString sDataName( pszDataName );

				// Look if string ends in "_vmat"
				if ( sDataName.FindAndReplace( szSuffix, ".vmat", sDataName.GetLen() - nSuffixCount ) )
				{
					bool bArray = false;
					FbxLayerElementArrayTemplate< void * > *pDirectArrayVoid = pFbxUserData->GetDirectArrayVoid( j, &bArray );
					if ( bArray && nPolygonCount == pDirectArrayVoid->GetCount() )
					{
						houdiniMatData.Insert( sDataName, pDirectArrayVoid );
					}
				}
			}
		}
	}

	if ( pMatElement )
	{
		if ( pMatElement->GetMappingMode() == FbxGeometryElement::eByPolygon )
		{
			nPolygonToFaceSetMap.RemoveAll();
			nPolygonToFaceSetMap.EnsureCapacity( nPolygonCount );

			CUtlVector< FbxString > materialList;
			materialList.SetCount( nMaterialCount );

			if ( houdiniMatData.Empty() )
			{
				for ( int i = 0; i < nPolygonCount; ++i )
				{
					nPolygonToFaceSetMap.AddToTail( pMatElement->GetIndexArray().GetAt( i ) );
				}
			}
			else
			{
				for ( int i = 0; i < nPolygonCount; ++i )
				{
					const int nPolygonMaterialIndex = pMatElement->GetIndexArray().GetAt( i );
					nPolygonToFaceSetMap.AddToTail( nPolygonMaterialIndex );

					if ( materialList[nPolygonMaterialIndex].IsEmpty() )
					{
						bool bFoundMat = false;

						for ( HoudiniMatDataMap_t::Iterator hmdIt = houdiniMatData.Begin(); hmdIt != houdiniMatData.End(); ++hmdIt )
						{
							FbxLayerElementArrayTemplate< void * > *pDirectArrayVoid = hmdIt->GetValue();
							int *pDirectArrayInt = NULL;

							pDirectArrayInt = pDirectArrayVoid->GetLocked( pDirectArrayInt, FbxLayerElementArray::eReadLock );
							if ( pDirectArrayInt[i] != 0 )
							{
								materialList[nPolygonMaterialIndex] = hmdIt->GetKey();
								bFoundMat = true;
							}
							pDirectArrayVoid->Release( reinterpret_cast< void ** >( &pDirectArrayInt ) );

							if ( bFoundMat )
								break;
						}
					}
				}
			}

			for ( int i = 0; i < nMaterialCount; ++i )
			{
				CUtlString sByPolygonMaterialPath;
				CUtlVector< CUtlString > materialSearchErrorList;

				if ( materialList[i].IsEmpty() )
				{
					FbxSurfaceMaterial *pFbxMaterial = pFbxMesh->GetNode()->GetMaterial( i );
					// If there's actually a material, change the fallback to the material name instead of the mesh name
					if ( pFbxMaterial )
					{
						sByPolygonMaterialPath = pFbxMaterial->GetName();
					}
					else
					{
						sByPolygonMaterialPath = sFallbackMaterialPath;
					}

					if ( !GetFbxMaterialPath( sByPolygonMaterialPath, pFbxMaterial, materialSearchErrorList ) )
					{
#ifdef SOURCE2
						AddConversionError( pDmeMesh->GetFileId(), CFmtStrMax( "GetFbxMaterialPath Failed! FbxMesh: %s FbxMaterial[%d]: %s, using: %s", pDmeMesh->GetName(), i, pFbxMaterial ? pFbxMaterial->GetName() : "nil", sByPolygonMaterialPath.Get() ).Get() );

						if ( !materialSearchErrorList.IsEmpty() )
						{
							AddConversionError( pDmeMesh->GetFileId(), " + Searched:" );

							for ( int e = 0; e < materialSearchErrorList.Count(); ++e )
							{
								AddConversionError( pDmeMesh->GetFileId(), CFmtStr( "   + %s", materialSearchErrorList[e].Get() ).Get() );
							}
						}
#endif
					}
				}
				else
				{
					sByPolygonMaterialPath = materialList[i].Buffer();
				}

				const CUtlString sByPolygonMaterialName = sByPolygonMaterialPath.GetBaseFilename();

				CDmeFaceSet *pDmeFaceSet = CreateElement< CDmeFaceSet >( sByPolygonMaterialName.String(), pDmeMesh->GetFileId() );
				CDmeMaterial *pDmeMaterial = CreateElement< CDmeMaterial >( sByPolygonMaterialName.String(), pDmeMesh->GetFileId() );
				pDmeMaterial->SetMaterial( sByPolygonMaterialPath.Get() );
				pDmeFaceSet->SetMaterial( pDmeMaterial );
				pDmeMesh->AddFaceSet( pDmeFaceSet );
			}

			return true;
		}
		else if ( pMatElement->GetMappingMode() == FbxGeometryElement::eAllSame )
		{
			CUtlVector< CUtlString > materialSearchErrorList;

			FbxSurfaceMaterial *pFbxMaterial = pFbxMesh->GetNode()->GetMaterial( 0 );
			if ( pFbxMaterial )
			{
				// If there's actually a material, change the fallback to the material name instead of the mesh name
				sMaterialPath = pFbxMaterial->GetName();
			}

			if ( !GetFbxMaterialPath( sMaterialPath, pFbxMaterial, materialSearchErrorList ) )
			{
#ifdef SOURCE2
				AddConversionError( pDmeMesh->GetFileId(), CFmtStrMax( "GetFbxMaterialPath Failed! FbxMesh: %s FbxMaterial: %s, using: %s", pDmeMesh->GetName(), pFbxMaterial ? pFbxMaterial->GetName() : "nil", sMaterialPath.Get() ).Get() );

				if ( !materialSearchErrorList.IsEmpty() )
				{
					AddConversionError( pDmeMesh->GetFileId(), " + Searched:" );

					for ( int e = 0; e < materialSearchErrorList.Count(); ++e )
					{
						AddConversionError( pDmeMesh->GetFileId(), CFmtStr( "   + %s", materialSearchErrorList[e].Get() ).Get() );
					}
				}
#endif
			}

			if ( houdiniMatData.GetSize() == 1 )
			{
				sMaterialPath = houdiniMatData.Begin()->GetKey();
			}
		}
		else
		{
			AddConversionError( pDmeMesh->GetFileId(), CFmtStrMax( "GetFbxMaterialPath Failed! FbxMesh: %s, Unsupported material mapping mode, using: %s", pDmeMesh->GetName(), sMaterialPath.Get() ).Get() );
		}
	}
	else
	{
		AddConversionError( pDmeMesh->GetFileId(), CFmtStrMax( "GetFbxMaterialPath Failed! FbxMesh: %s, No FbxMaterial assigned, using: %s", pDmeMesh->GetName(), sMaterialPath.Get() ).Get() );
	}

	// Either a single material or no material on the mesh
	static const int nZero = 0;

	nPolygonToFaceSetMap.SetCount( nPolygonCount );
	nPolygonToFaceSetMap.FillWithValue( nZero );

	const CUtlString sMaterialName = sMaterialPath.GetBaseFilename();

	CDmeFaceSet *pDmeFaceSet = CreateElement< CDmeFaceSet >( sMaterialName.String(), pDmeMesh->GetFileId() );
	CDmeMaterial *pDmeMaterial = CreateElement< CDmeMaterial >( sMaterialName.String(), pDmeMesh->GetFileId() );
	pDmeMaterial->SetMaterial( sMaterialPath.String() );
	pDmeFaceSet->SetMaterial( pDmeMaterial );
	pDmeMesh->AddFaceSet( pDmeFaceSet );

	return true;
}


//-----------------------------------------------------------------------------
//
// Search for a material by name by generating resource names and seeing if
// the resource exists on disk in content and then game and then on user
// specified material search paths
//
// If a material file cannot be found, the original name is copied and false
// is returned
//
//-----------------------------------------------------------------------------
bool CDmFbxSerializer::FindMaterialResource( CUtlString &sOutMaterialPath, const char *pszInMaterialName, CUtlVector< CUtlString > &materialSearchErrorList ) const
{
#ifdef SOURCE2
	char szResourceName[MAX_PATH] = { 0 };
	char szResourceFullPath[MAX_PATH] = { 0 };
	char szMaterialPath[MAX_PATH] = { 0 };

	ResourcePathGenerationType_t pSearchPaths[] =
	{
		RESOURCE_PATH_CONTENT,
		RESOURCE_PATH_GAME
	};

	const char *szSearchPaths[] =
	{
		"CONTENT",
		"GAME"
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE( pSearchPaths ) == ARRAYSIZE( szSearchPaths ) );

	FixupResourceName( pszInMaterialName, RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) );

	for ( int s = 0; s < ARRAYSIZE( pSearchPaths ); ++s )
	{
		// Check if current material is valid
		if ( GenerateStandardFullPathForResourceName( szResourceName, pSearchPaths[s], szResourceFullPath, ARRAYSIZE( szResourceFullPath ) ) )
		{
			sOutMaterialPath = szResourceName;
			return true;
		}
		else
		{
			materialSearchErrorList.AddToTail( CFmtStr( "%-7s %s", szSearchPaths[s], szResourceName ).Get() );
		}
	}

	// Loop through material search paths and try to find the material
	for ( int s = 0; s < ARRAYSIZE( pSearchPaths ); s++ )
	{
		for ( int i = 0; i < m_sOptMaterialSearchPathList.Count(); ++i )
		{
			V_ComposeFileName( m_sOptMaterialSearchPathList[i].Get(), pszInMaterialName, szMaterialPath, ARRAYSIZE( szMaterialPath ) );
			FixupResourceName( szMaterialPath, RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) );

			if ( GenerateStandardFullPathForResourceName( szResourceName, pSearchPaths[s], szResourceFullPath, ARRAYSIZE( szResourceFullPath ) ) )
			{
				sOutMaterialPath = szResourceName;
				return true;
			}
			else
			{
				materialSearchErrorList.AddToTail( CFmtStr( "%-7s %s", szSearchPaths[s], szResourceName ).Get() );
			}
		}
	}
	return false;
#else
	//sOutMaterialPath = CUtlString( pszInMaterialName ).UnqualifiedFilename();
	return false;
#endif
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmFbxSerializer::GetFbxMaterialPathFromFbxFileTexture( CUtlString &sMaterialPath, FbxFileTexture *pFileTexture, CUtlVector< CUtlString > &materialSearchErrorList ) const
{
	const char *pszFileName = pFileTexture->GetFileName();
	if ( !pszFileName || *pszFileName == '\0' )
		return false;

#ifdef SOURCE2
	char szResourceName[ MAX_PATH ];
	GenerateResourceNameFromFileName( pszFileName, RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) );
#else
	sMaterialPath = CUtlString( pszFileName ).UnqualifiedFilename();
	return true;

	const char *szResourceName;
	szResourceName = pszFileName;
#endif

	if ( FindMaterialResource( sMaterialPath, szResourceName, materialSearchErrorList ) )
		return true;

	// See if the texture ends in any well known suffixes

	const char *szSuffixes[] = { "_color." };

	FbxString sTmpResourceName = szResourceName;

	for ( int i = 0; i < ARRAYSIZE( szSuffixes ); ++i )
	{
		if ( sTmpResourceName.FindAndReplace( szSuffixes[ i ], "." ) )
		{
			if ( FindMaterialResource( sMaterialPath, sTmpResourceName.Buffer(), materialSearchErrorList ) )
				return true;

			if ( !V_strcmp( szSuffixes[i], "_color." ) )
			{
				// If we got here, probably just use this one, likely the file wasn't put onto disk yet
				sMaterialPath = sTmpResourceName.Buffer();
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmFbxSerializer::GetFbxMaterialPath( CUtlString &sMaterialPath, FbxSurfaceMaterial *pFbxMat, CUtlVector< CUtlString > &materialSearchErrorList ) const
{
	if ( !pFbxMat )
	{
		materialSearchErrorList.AddToTail( CFmtStr( "GetFbxMaterialPath Failed: No Fbx material" ).Get() );
		return false;
	}

	// See if FBX_vmatPath is explicitly set, if it is, then just use it and return
	static const char *szExplitPaths[] = { "vmatPath", "FBX_vmatPath" };

	for ( int i = 0; i < ARRAYSIZE( szExplitPaths ); ++i )
	{
		const char *pszExplicitPath = szExplitPaths[i];

		FbxProperty fbx_vmatPath = pFbxMat->FindProperty( pszExplicitPath );
		if ( fbx_vmatPath.IsValid() )
		{
			const EFbxType eFbxType = fbx_vmatPath.GetPropertyDataType().GetType();
			if ( eFbxType == eFbxString )
			{
				const FbxString sVMatPath = fbx_vmatPath.Get< FbxString >();
				if ( !sVMatPath.IsEmpty() )
				{
					sMaterialPath = sVMatPath.Buffer();
					return true;
				}
				else
				{
					materialSearchErrorList.AddToTail( CFmtStr( "FBXSurfaceMaterial( %s ).FBX_vmatPath found but empty", pFbxMat->GetName() ).Get() );
				}
			}
			else
			{
				materialSearchErrorList.AddToTail( CFmtStr( "FBXSurfaceMaterial( %s ).FBX_vmatPath found but not a string attribute", pFbxMat->GetName() ).Get() );
			}
		}
	}

	// See if the name of the material maps directly to a material file
	if ( FindMaterialResource( sMaterialPath, pFbxMat->GetNameOnly(), materialSearchErrorList ) )
		return true;

	// Look to see if a texture can map to a .vmat
	FbxProperty fbxProperty = pFbxMat->FindProperty( FbxSurfaceMaterial::sDiffuse );

	const int nLayeredTextureCount = fbxProperty.GetSrcObjectCount( FbxLayeredTexture::ClassId );
	if ( nLayeredTextureCount > 0 )
	{
		for ( int j = 0; j < nLayeredTextureCount; ++j )
		{
			FbxLayeredTexture *pLayeredTexture = FbxCast< FbxLayeredTexture >( fbxProperty.GetSrcObject( FbxLayeredTexture::ClassId, j ) );
			const int nTexCount = pLayeredTexture->GetSrcObjectCount( FbxTexture::ClassId );

			for ( int k = 0; k < nTexCount; ++k )
			{
				FbxFileTexture *pFileTexture = FbxCast< FbxFileTexture >( pLayeredTexture->GetSrcObject( FbxTexture::ClassId, k ) );

				if ( pFileTexture )
				{
					if ( GetFbxMaterialPathFromFbxFileTexture( sMaterialPath, pFileTexture, materialSearchErrorList ) )
						return	true;
				}
			}
		}
	}
	else
	{
		//no layered texture simply get on the property
		const int nTexCount = fbxProperty.GetSrcObjectCount( FbxTexture::ClassId );

		if ( nTexCount > 0 )
		{
			for ( int j = 0; j < nTexCount; ++j )
			{
				FbxFileTexture *pFileTexture = FbxCast< FbxFileTexture >( fbxProperty.GetSrcObject( FbxTexture::ClassId, j ) );

				if ( pFileTexture )
				{
					if ( GetFbxMaterialPathFromFbxFileTexture( sMaterialPath, pFileTexture, materialSearchErrorList ) )
						return	true;
				}
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::SkinMeshes_R(
	const FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, FbxNode *pFbxNode ) const
{
	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();

	if ( pFbxNodeAttribute )
	{
		const FbxNodeAttribute::EType nAttributeType = pFbxNodeAttribute->GetAttributeType();
		if ( nAttributeType == FbxNodeAttribute::eMesh )
		{
			FbxToDmxMap_t::IndexType_t nDmxIndex = fbxToDmxMap.Find( pFbxNode );
			if ( fbxToDmxMap.IsValidIndex( nDmxIndex ) )
			{
				SkinMesh( fbxToDmxMap.Element( nDmxIndex ), fbxToDmxMap, pDmeModel, pFbxNode );
			}
		}
	}

	for ( int i = 0; i < pFbxNode->GetChildCount(); ++i )
	{
		SkinMeshes_R( fbxToDmxMap, pDmeModel, pFbxNode->GetChild( i ) );
	}
}


//=============================================================================
//
//=============================================================================
class CVertexSkin
{
public:
	enum
	{
		kMaxWeights = 3
	};

	enum SkinError_t
	{
		kSkinErrorNone = 0,
		kSkinErrorTooManyWeights,
		kSkinErrorZeroWeight,
		kSkinErrorNegativeWeight
	};

	bool AddWeight( int nJointIndex, float flJointWeight )
	{
		// Ignore 0 weights
		if ( flJointWeight < m_flEps )
			return false;

		IndexWeightPair_t &indexWeightPair = m_weights[ m_weights.AddToTail() ];
		indexWeightPair.m_nIndex = nJointIndex;
		indexWeightPair.m_flWeight = flJointWeight;

		return true;
	}

	int GetIndex( int nIndex ) const
	{
		if ( nIndex >= kMaxWeights || nIndex >= m_weights.Count() )
			return -1;

		return m_weights[nIndex].m_nIndex;
	}

	float GetWeight( int nIndex ) const
	{
		if ( nIndex >= kMaxWeights || nIndex >= m_weights.Count() )
			return 0.0f;

		return m_weights[nIndex].m_flWeight;
	}
	
	SkinError_t Renormalize();

protected:
	static int VertexWeightLessFunc( const void *pLhs, const void *pRhs );
	static const float m_flEps;

	struct IndexWeightPair_t
	{
		int m_nIndex;
		float m_flWeight;
	};

	CUtlVector< IndexWeightPair_t > m_weights;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const float CVertexSkin::m_flEps = 1.0e-4;


//-----------------------------------------------------------------------------
// Used by HandleVertexWeights, sorts vertex weights by weight
//-----------------------------------------------------------------------------
int CVertexSkin::VertexWeightLessFunc( const void *pLhs, const void *pRhs )
{
	const IndexWeightPair_t *pVertexWeightL = reinterpret_cast< const IndexWeightPair_t * >( pLhs );
	const IndexWeightPair_t *pVertexWeightR = reinterpret_cast< const IndexWeightPair_t * >( pRhs );

	if ( pVertexWeightL->m_nIndex < 0 )
	{
		if ( pVertexWeightR->m_nIndex < 0 )
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else if ( pVertexWeightR->m_nIndex < 0 )
	{
		return -1;
	}

	if ( pVertexWeightL->m_flWeight > pVertexWeightR->m_flWeight )
	{
		return -1;
	}
	else if ( pVertexWeightL->m_flWeight < pVertexWeightR->m_flWeight )
	{
		return 1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
//
// Returns: kSkinErrorNone if all ok
//          kSkinErrorTooManyWeights if more than kMaxWeights
//			kSkinErrorZeroWeight if total weight is 0 for this vertex
//			kSkinErrorNegativeWeight if total weight is < 0 for this vertex
//
//-----------------------------------------------------------------------------
CVertexSkin::SkinError_t CVertexSkin::Renormalize()
{
	SkinError_t nErr = kSkinErrorNone;

	// Sort by weight, largest weights first
	qsort( m_weights.Base(), m_weights.Count(), sizeof( IndexWeightPair_t ), VertexWeightLessFunc );

	// Remove any weights > kMaxWeights
	if ( m_weights.Count() > kMaxWeights )
	{
		m_weights.RemoveMultipleFromTail( m_weights.Count() - kMaxWeights );
		nErr = kSkinErrorTooManyWeights;
	}

	float flTotalWeight = 0.0f;
	for ( int i = 0; i < m_weights.Count(); ++i )
	{
		flTotalWeight += m_weights[i].m_flWeight;
	}

	if ( fabs( flTotalWeight ) < m_flEps )
	{
		nErr = kSkinErrorZeroWeight;
	}
	else
	{
		if ( flTotalWeight < 0.0f )
		{
			nErr = kSkinErrorNegativeWeight;
		}

		if ( fabs( fabs( flTotalWeight ) - 1.0f ) > m_flEps )
		{
			for ( int i = 0; i < m_weights.Count(); ++i )
			{
				m_weights[i].m_flWeight /= flTotalWeight;
			}
		}
	}

	return nErr;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::SkinMesh( CDmeDag *pDmeDag, const FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, FbxNode *pFbxNode ) const
{
	static const char *szClusterModes[] = { "Normalize", "Additive", "Total1" };

	if ( !pDmeDag )
		return;

	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();
	if ( !pFbxNodeAttribute || pFbxNodeAttribute->GetAttributeType() != FbxNodeAttribute::eMesh )
		return;

	FbxMesh *pFbxMesh = reinterpret_cast< FbxMesh * >( pFbxNodeAttribute );
	if ( !pFbxMesh )
		return;

	CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
	if ( !pDmeMesh )
		return;

	CDmeVertexData *pDmeVertexData = pDmeMesh->GetBindBaseState();
	if ( !pDmeVertexData )
	{
		Warning( "Warning! No DmeVertexData on DmeMesh (%s)\n", pDmeMesh->GetName() );
		return;
	}

	const int nPositionCount = pDmeVertexData->GetPositionData().Count();
	if ( nPositionCount <= 0 )
	{
		Warning( "Warning! Invalid position count (%d) on DmeMesh (%s)\n", nPositionCount, pDmeMesh->GetName() );
		return;
	}

	const int nDeformerCount = pFbxMesh->GetDeformerCount( FbxDeformer::eSkin );
	if ( nDeformerCount <= 0 )
		return;

	if ( Verbose2() )
	{
		Msg( " * Skinning Mesh: %s\n", pDmeDag->GetName() );
	}

	int nJointCount = 0;

	CUtlVector< CVertexSkin > vertexSkinWeights;
	vertexSkinWeights.SetCount( nPositionCount );

	CDmElement *pDmRoot = pDmeModel->GetValueElement< CDmElement >( "__rootElement" );

	for ( int i = 0; i < nDeformerCount; ++i )
	{
		FbxSkin *pFbxSkin = FbxCast< FbxSkin >( pFbxMesh->GetDeformer( i, FbxDeformer::eSkin ) );
		if ( !pFbxSkin )
			continue;

		const int nClusterCount = pFbxSkin->GetClusterCount();
		for ( int j = 0; j < nClusterCount; ++j )
		{
			FbxCluster *pFbxCluster = pFbxSkin->GetCluster( j );
			FbxNode *pFbxLinkNode = pFbxCluster->GetLink();

			if ( !pFbxLinkNode )
				continue;

			const FbxToDmxMap_t::IndexType_t nDmxIndex = fbxToDmxMap.Find( pFbxLinkNode );
			if ( !fbxToDmxMap.IsValidIndex( nDmxIndex ) )
			{
				Warning( "Warning! FBX mesh (%s) skinned to unmapped node (%s)\n", pFbxNode->GetName(), pFbxLinkNode->GetName() );
				continue;
			}

			CDmeJoint *pDmeJoint = CastElement< CDmeJoint >( fbxToDmxMap.Element( nDmxIndex ) );
			if ( !pDmeJoint )
			{
				Warning( "Warning! FBX mesh (%s) skinned to non-joint (%s)\n", pFbxNode->GetName(), pFbxLinkNode->GetName() );
				continue;
			}

			const int nJointIndex = pDmeModel->GetJointIndex( pDmeJoint );
			if ( nJointIndex < 0 )
			{
				Warning( "Warning! FBX mesh (%s) skinned to joint (%s) which isn't in DmeModel\n", pFbxNode->GetName(), pDmeJoint->GetName() );
				continue;
			}

			const FbxCluster::ELinkMode nLinkMode = pFbxCluster->GetLinkMode();
			if ( nLinkMode != FbxCluster::eNormalize && nLinkMode != FbxCluster::eTotalOne )
			{
				Warning( "Warning! FBX mesh (%s) skinned to joint (%s) but mode %s isn't supported, only %s & %s\n", pFbxNode->GetName(), pDmeJoint->GetName(), szClusterModes[nLinkMode], szClusterModes[FbxCluster::eNormalize], szClusterModes[FbxCluster::eTotalOne] );
				continue;
			}

			const int nIndexCount = pFbxCluster->GetControlPointIndicesCount();
			const int *pnIndices = pFbxCluster->GetControlPointIndices();
			const double *pfWeights = pFbxCluster->GetControlPointWeights();

			if ( nIndexCount > 0 )
			{
				++nJointCount;

				if ( Verbose2() )
				{
					Msg( "   + DmeJoint[%3d] %5d Vertices - %s\n",
						nJointIndex, nIndexCount, pDmeJoint->GetName() );
				}
			}

			for ( int k = 0; k < nIndexCount; ++k )
			{
				const int nVertexIndex = pnIndices[k];
				Assert( nVertexIndex >= 0 && nVertexIndex <= vertexSkinWeights.Count() );
				CVertexSkin &vertexSkinWeight = vertexSkinWeights[ nVertexIndex ];
				vertexSkinWeight.AddWeight( nJointIndex, pfWeights[k] );
			}
		}

		CUtlVector< CVertexSkin::SkinError_t > errorsAdded;

		// Clean up skin weights
		for ( int j = 0; j < vertexSkinWeights.Count(); ++j )
		{
			const CVertexSkin::SkinError_t nErr = vertexSkinWeights[j].Renormalize();

			if ( nErr == CVertexSkin::kSkinErrorNone )
				continue;

			// Do a linear search, maximum of 3 elements and exit early if this error is already added, AddConversionError does a string compare to an array
			if ( errorsAdded.HasElement( nErr ) )
				continue;

			errorsAdded.AddToTail( nErr );

			switch ( nErr )
			{
			case CVertexSkin::kSkinErrorTooManyWeights:
				AddConversionError( pDmRoot->GetFileId(), CFmtStr( "Too many skin weights on some vertices, maximum is %d weights per vertex, will renormalize and discard smallest weights", CVertexSkin::kMaxWeights ).Access() );
				break;
			case CVertexSkin::kSkinErrorZeroWeight:
				AddConversionError( pDmRoot->GetFileId(), "Zero skin weights on one or more vertices" );
				break;
			case CVertexSkin::kSkinErrorNegativeWeight:
				AddConversionError( pDmRoot->GetFileId(), "Negative total skin weights on one or more vertices" );
				break;
			}
		}

		CUtlVector< int > jointIndices;
		jointIndices.EnsureCapacity( CVertexSkin::kMaxWeights * nPositionCount );

		CUtlVector< float > jointWeights;
		jointWeights.EnsureCapacity( CVertexSkin::kMaxWeights * nPositionCount );

		for ( int j = 0; j < vertexSkinWeights.Count(); ++j )
		{
			const CVertexSkin &vertexSkinWeight = vertexSkinWeights[ j ];

			for ( int k = 0; k < CVertexSkin::kMaxWeights; ++k )
			{
				jointIndices.AddToTail( vertexSkinWeight.GetIndex( k ) );
				jointWeights.AddToTail( vertexSkinWeight.GetWeight( k ) );
			}
		}

		FieldIndex_t nJointWeightsIndex;
		FieldIndex_t nJointIndicesIndex;

		pDmeVertexData->CreateJointWeightsAndIndices( CVertexSkin::kMaxWeights, &nJointWeightsIndex, &nJointIndicesIndex );
		Assert( jointIndices.Count() == CVertexSkin::kMaxWeights * nPositionCount );
		Assert( jointWeights.Count() == CVertexSkin::kMaxWeights * nPositionCount );

		pDmeVertexData->AddVertexData( nJointIndicesIndex, jointIndices.Count() );
		pDmeVertexData->SetVertexData( nJointIndicesIndex, 0, jointIndices.Count(), AT_INT, jointIndices.Base() );

		pDmeVertexData->AddVertexData( nJointWeightsIndex, jointWeights.Count() );
		pDmeVertexData->SetVertexData( nJointWeightsIndex, 0, jointWeights.Count(), AT_FLOAT, jointWeights.Base() );

		break;	// Only do the first skin deformer
	}

	if ( Verbose1() && !Verbose2() && nJointCount )
	{
		Msg( " * Skinning Mesh Joints %3d: %s\n", nJointCount, pDmeDag->GetName() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::AddBlendShapes_R( const FbxToDmxMap_t &fbxToDmxMap, CDmElement *pDmeRoot, FbxNode *pFbxNode ) const
{
	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();

	if ( pFbxNodeAttribute )
	{
		const FbxNodeAttribute::EType nAttributeType = pFbxNodeAttribute->GetAttributeType();
		if ( nAttributeType == FbxNodeAttribute::eMesh )
		{
			FbxToDmxMap_t::IndexType_t nDmxIndex = fbxToDmxMap.Find( pFbxNode );
			if ( fbxToDmxMap.IsValidIndex( nDmxIndex ) )
			{
				AddBlendShape( fbxToDmxMap.Element( nDmxIndex ), fbxToDmxMap, pDmeRoot, pFbxNode );
			}
		}
	}

	for ( int i = 0; i < pFbxNode->GetChildCount(); ++i )
	{
		AddBlendShapes_R( fbxToDmxMap, pDmeRoot, pFbxNode->GetChild( i ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::AddBlendShape( CDmeDag *pDmeDag, const FbxToDmxMap_t &fbxToDmxMap, CDmElement *pDmeRoot, FbxNode *pFbxNode ) const
{
	if ( !pDmeDag )
		return;

	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();
	if ( !pFbxNodeAttribute || pFbxNodeAttribute->GetAttributeType() != FbxNodeAttribute::eMesh )
		return;

	FbxMesh *pFbxMesh = reinterpret_cast< FbxMesh * >( pFbxNodeAttribute );
	if ( !pFbxMesh )
		return;

	CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
	if ( !pDmeMesh )
		return;

	CDmeVertexData *pDmeVertexData = pDmeMesh->GetBindBaseState();
	if ( !pDmeVertexData )
	{
		Warning( "Warning! No DmeVertexData on DmeMesh (%s)\n", pDmeMesh->GetName() );
		return;
	}

	const CUtlVector< Vector > &positionData = pDmeVertexData->GetPositionData();
	const int nPositionCount = positionData.Count();
	if ( nPositionCount <= 0 )
	{
		Warning( "Warning! Invalid position count (%d) on DmeMesh (%s)\n", nPositionCount, pDmeMesh->GetName() );
		return;
	}

	const int nBlendShapeCount = pFbxMesh->GetDeformerCount( FbxDeformer::eBlendShape );
	if ( nBlendShapeCount <= 0 )
		return;


	CUtlVector< Vector > vDeltas;
	CUtlVector< int > nDeltaIndices;

	CDmeCombinationOperator *pDmeComboOp = FindOrCreateComboOp( pDmeRoot );
	if ( !pDmeComboOp )
	{
		Warning( "Warning! Couldn't create DmeComboOp for FbxMesh %s\n", pFbxNode->GetName() );
		return;
	}

	for ( int nBlendShapeIndex = 0; nBlendShapeIndex < nBlendShapeCount; ++nBlendShapeIndex )
	{
		FbxBlendShape *pFbxBlendShape = FbxCast< FbxBlendShape >( pFbxMesh->GetDeformer(nBlendShapeIndex, FbxDeformer::eBlendShape) );
		const int nBlendShapeChannelCount = pFbxBlendShape->GetBlendShapeChannelCount();

		if ( Verbose1() )
		{
			if ( nBlendShapeCount > 1 )
			{
				Msg( " * BlendShape Mesh: Shapes %3d - BlendShape %3d/%3d - %s\n",
					nBlendShapeChannelCount, nBlendShapeIndex + 1, nBlendShapeCount, pDmeMesh->GetName() );
			}
			else
			{
				Msg( " * BlendShape Mesh: Shapes %3d - %s\n",
					nBlendShapeChannelCount, pDmeMesh->GetName() );
			}
		}

		for ( int nBlendShapeChannelIndex = 0; nBlendShapeChannelIndex < nBlendShapeChannelCount; ++nBlendShapeChannelIndex )
		{
			FbxBlendShapeChannel *pFbxBlendShapeChannel = pFbxBlendShape->GetBlendShapeChannel( nBlendShapeChannelIndex );
			const char *pszDeltaName = pFbxBlendShapeChannel->GetName();
			const char *pszExt = V_GetFileExtension( pszDeltaName );		// FBX will probably name this blendShape.<THING> we just want <THING>
			if ( pszExt )
			{
				pszDeltaName = pszExt;
			}

			CUtlString sDeltaName;
			CleanupName( sDeltaName, pszDeltaName );

			const int nTargetShapeCount = pFbxBlendShapeChannel->GetTargetShapeCount();

			for ( int nTargetShapeIndex = 0; nTargetShapeIndex < nTargetShapeCount; ++nTargetShapeIndex )
			{
				FbxShape *pFbxShape = pFbxBlendShapeChannel->GetTargetShape( nTargetShapeIndex );

				const int nControlPointsCount = pFbxShape->GetControlPointsCount();
				const FbxVector4 *pvControlPoints = pFbxShape->GetControlPoints();

				const int nControlPointIndicesCount = pFbxShape->GetControlPointIndicesCount();
				const int *pnControlPointIndices = pFbxShape->GetControlPointIndices();

				// pvControlPoints has as many values as the original mesh but only the vertices indexed by pnControlPointIndices are deltas
				Assert( nControlPointsCount == nPositionCount );

				if ( nControlPointsCount == nPositionCount )
				{
					vDeltas.SetCount( nControlPointIndicesCount );
					nDeltaIndices.SetCount( nControlPointIndicesCount );

					for ( int i = 0; i < nControlPointIndicesCount; ++i )
					{
						const int nDeltaIndex = pnControlPointIndices[i];

						const Vector &vDme = positionData[ nDeltaIndex ];
						const FbxVector4 &vFbxSrc = pvControlPoints[ nDeltaIndex ];
						vDeltas[i] = Vector( vFbxSrc[0] - vDme.x, vFbxSrc[1] - vDme.y, vFbxSrc[2] - vDme.z );
						nDeltaIndices[i] = nDeltaIndex;
					}
				}
				else
				{
					Warning( "Warning! FbxMesh %s Has Unhandled FbxBlendShapeChannel %s\n", pFbxNode->GetName(), pszDeltaName );
				}

				CDmeVertexDeltaData *pDmeVertexDeltaData = pDmeMesh->FindOrCreateDeltaState( pszDeltaName, m_bOptUnderscoreForCorrectors );
				pszDeltaName = pDmeVertexDeltaData->GetName();	// The delta name could be changed if m_bOptUnderscoreForCorrectors is on, i.e. B_A becomes A_B
				pDmeVertexDeltaData->FlipVCoordinate( true );

				FieldIndex_t nDeltaPosIndex = pDmeVertexDeltaData->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
				pDmeVertexDeltaData->AddVertexData( nDeltaPosIndex, vDeltas.Count() );

				pDmeVertexDeltaData->SetVertexData( nDeltaPosIndex, 0, vDeltas.Count(), AT_VECTOR3, vDeltas.Base() );
				pDmeVertexDeltaData->SetVertexIndices( nDeltaPosIndex, 0, nDeltaIndices.Count(), nDeltaIndices.Base() );

				if ( FindOrCreateControl( pDmeComboOp, pszDeltaName ) )
				{
					pDmeVertexDeltaData->SetValue( "corrected", true );
				}

				// TODO: See if FBX handle exporting expressions?  Face rules?
				// TODO: Handle normals

				if ( Verbose2() )
				{
					Msg( "   + Shape %3d Deltas: %5d - %s\n",
						nBlendShapeChannelIndex, nControlPointIndicesCount, sDeltaName.String() );
				}

				AssertMsg( nTargetShapeCount == 1, "TODO: Handle multiple target shapes?" );
				break;
			}
		}
	}

	pDmeComboOp->AddTarget( pDmeMesh );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeCombinationOperator * CDmFbxSerializer::FindOrCreateComboOp( CDmElement *pDmeRoot ) const
{
	CDmeCombinationOperator *pDmeComboOp = pDmeRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );

	if ( !pDmeComboOp )
	{
		pDmeComboOp = CreateElement< CDmeCombinationOperator >( "combinationOperator", pDmeRoot->GetFileId() );
		pDmeRoot->SetValue( "combinationOperator", pDmeComboOp );
	}

	return pDmeComboOp;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmFbxSerializer::FindOrCreateControl( CDmeCombinationOperator *pDmeComboOp, const char *pszName ) const
{
	if ( m_bOptUnderscoreForCorrectors )
	{
		if ( strchr( pszName, '_' ) )		// Don't create a control if m_bOptUnderscoreForCorrectors is true and name has '_' in it
			return false;
	}

	const ControlIndex_t nControlIndex = pDmeComboOp->FindOrCreateControl( pszName, false, true );

	return nControlIndex >= 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CUtlString CleanupName( const char *pszName )
{
	CUtlString sCleanName( pszName );

	const int nNameLen = V_strlen( pszName );
	if ( nNameLen > 1 )
	{
		const char *pszColon = V_strrchr( pszName, ':' );
		if ( pszColon && nNameLen > ( pszColon - pszName + 1 ) && *( pszColon + 1 ) )
		{
			sCleanName.Set( pszColon + 1 );
		}
	}

	return sCleanName;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CUtlString GetName( const FbxNode *pFbxNode )
{
	const FbxString sName = pFbxNode->GetNameOnly();
	const char *pszName = sName.Buffer();

	return CleanupName( pszName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::GetName( CUtlString &sCleanName, const FbxNode *pFbxNode ) const
{
	FbxString sName = pFbxNode->GetNameOnly();
	const char *pszName = sName.Buffer();

	sCleanName = ::CleanupName( pszName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::CleanupName( CUtlString &sCleanName, const char *pszName ) const
{
	sCleanName = ::CleanupName( pszName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
struct FbxDmxAnimData_t
{
	FbxDmxAnimData_t()
	: m_pFbxNode( nullptr )
	, m_pDmePosLog( nullptr )
	, m_pDmeRotLog( nullptr )
	, m_pParent( nullptr )
	, m_pFbxAnimCurve( nullptr )
	, m_pDmeFloatLog( nullptr )
	, m_flMin( 0.0f )
	, m_flMax( 1.0f )
	, m_bNormalize( false )
	{
		SetIdentityMatrix( m_mWorldInverse );
	}

	void SetWorldMatrix( const matrix3x4_t &mWorld )
	{
		MatrixInvert( mWorld, m_mWorldInverse );
	}

	FbxNode *m_pFbxNode;
	CDmeVector3Log *m_pDmePosLog;
	CDmeQuaternionLog *m_pDmeRotLog;

	FbxDmxAnimData_t *m_pParent;
	matrix3x4_t m_mWorldInverse;

	FbxProperty m_fbxProperty;
	FbxAnimCurve *m_pFbxAnimCurve;
	CDmeFloatLog *m_pDmeFloatLog;

	bool m_bNormalize;
	float m_flMin;
	float m_flMax;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeFloatLog *CreateMorphChannel( const char *pszChannelName, CDmeChannelsClip *pDmeChannelsClip )
{
	CDmeChannel *pDmeFloatChannel = CreateElement< CDmeChannel >( CFmtStr( "%s_flex_channel", pszChannelName ).Access(), pDmeChannelsClip->GetFileId() );
	pDmeFloatChannel->SetMode( CM_PLAY );

	CDmElement *pDmeToElement = CreateElement< CDmElement >( pszChannelName, pDmeFloatChannel->GetFileId() );
	pDmeToElement->SetValue( "flexWeight", 0.0f );
	pDmeFloatChannel->SetOutput( pDmeToElement, "flexWeight" );

	CDmeFloatLog *pDmeFloatLog = pDmeFloatChannel->CreateLog< float >();
	pDmeFloatLog->SetValueThreshold( 1.0e-6 );
	pDmeFloatChannel->SetValue( "channelType", "morph" );
	pDmeChannelsClip->m_Channels.AddToTail( pDmeFloatChannel );

	return pDmeFloatLog;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::ComputeVstFlexSliderAnimDataList_R(
	FbxAnimLayer *pFbxAnimLayer,
	CUtlVector< FbxDmxAnimData_t * > &animDataList,
	CDmeChannelsClip *pDmeChannelsClip,
	const CDmFbxSerializer::FbxToDmxMap_t &fbxToDmxMap,
	FbxNode *pFbxNode ) const
{
	CDmFbxSerializer::FbxToDmxMap_t::IndexType_t nIndex = fbxToDmxMap.Find( pFbxNode );
	if ( !fbxToDmxMap.IsValidIndex( nIndex ) )
		return;

	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();
	if ( pFbxNodeAttribute )
	{
		const FbxNodeAttribute::EType nAttributeType = pFbxNodeAttribute->GetAttributeType();
		if ( nAttributeType == FbxNodeAttribute::eNull )
		{
			const CUtlString sNodeName = ::GetName( pFbxNode );
			if ( sNodeName == "vstFlexSlider" || pFbxNode->FindProperty( "vstFlexSlider", false ).IsValid() )
			{
				for ( FbxProperty fbxProperty = pFbxNode->GetFirstProperty(); fbxProperty.IsValid(); fbxProperty = pFbxNode->GetNextProperty( fbxProperty ) )
				{
					const FbxDataType fbxDataType = fbxProperty.GetPropertyDataType();
					const EFbxType eFbxType = fbxDataType.GetType();

					if ( fbxProperty.GetFlag( FbxPropertyFlags::eUserDefined ) && ( eFbxType == eFbxDouble || eFbxType == eFbxFloat ) )
					{
						const char *pszChannelName = fbxProperty.GetNameAsCStr();

						FbxDmxAnimData_t *pAnimData = animDataList[ animDataList.AddToTail( new FbxDmxAnimData_t ) ];
						animDataList.AddToTail( pAnimData );
						pAnimData->m_fbxProperty = fbxProperty;

						if ( fbxProperty.HasMinLimit() && fbxProperty.HasMaxLimit() )
						{
							pAnimData->m_bNormalize = true;
							pAnimData->m_flMin = fbxProperty.GetMinLimit();
							pAnimData->m_flMax = fbxProperty.GetMaxLimit();
						}

						pAnimData->m_pDmeFloatLog = CreateMorphChannel( pszChannelName, pDmeChannelsClip );

						if ( Verbose1() )
						{
							Msg( "     - vstFlexSlider FlexControl Channel: %s\n", pszChannelName );
						}
					}
				}
			}
		}
	}

	for ( int i = 0; i < pFbxNode->GetChildCount(); ++i )
	{
		ComputeVstFlexSliderAnimDataList_R( pFbxAnimLayer, animDataList, pDmeChannelsClip, fbxToDmxMap, pFbxNode->GetChild( i ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::ComputeAnimDataList_R(
	FbxAnimLayer *pFbxAnimLayer,
	CUtlVector< FbxDmxAnimData_t * > &animDataList,
	CDmeChannelsClip *pDmeChannelsClip,
	const CDmFbxSerializer::FbxToDmxMap_t &fbxToDmxMap,
	FbxNode *pFbxNode,
	FbxDmxAnimData_t *pAnimDataParent ) const
{
	CDmFbxSerializer::FbxToDmxMap_t::IndexType_t nIndex = fbxToDmxMap.Find( pFbxNode );
	if ( !fbxToDmxMap.IsValidIndex( nIndex ) )
		return;

	FbxNodeAttribute *pFbxNodeAttribute = pFbxNode->GetNodeAttribute();
	if ( pFbxNodeAttribute && pFbxNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh )
	{
		FbxGeometry *pFbxGeometry = static_cast< FbxGeometry * >( pFbxNodeAttribute );
		const int nBlendShapeDeformerCount = pFbxGeometry->GetDeformerCount( FbxDeformer::eBlendShape );
		for ( int lBlendShapeIndex = 0; lBlendShapeIndex < nBlendShapeDeformerCount; ++lBlendShapeIndex )
		{
			FbxBlendShape *pFbxBlendShape = static_cast< FbxBlendShape * >( pFbxGeometry->GetDeformer( lBlendShapeIndex, FbxDeformer::eBlendShape ) );
			if ( pFbxBlendShape )
			{
				const int nBlendShapeChannelCount = pFbxBlendShape->GetBlendShapeChannelCount();
				for ( int nChannelIndex = 0; nChannelIndex < nBlendShapeChannelCount; ++nChannelIndex )
				{
					FbxBlendShapeChannel *pFbxBlendShapeChannel = pFbxBlendShape->GetBlendShapeChannel( nChannelIndex );
					const CUtlString sChannelName = ::CleanupName( pFbxBlendShapeChannel->GetName() );
					const char *pszChannelName = sChannelName.Get();
					const char *pszExt = V_GetFileExtension( pszChannelName );		// FBX will probably name this blendShape.<THING> we just want <THING>
					if ( pszExt )
					{
						pszChannelName = pszExt;
					}

					FbxAnimCurve *pFbxAnimCurve = pFbxGeometry->GetShapeChannel( lBlendShapeIndex, nChannelIndex, pFbxAnimLayer, true );
					if ( pFbxAnimCurve )
					{
						bool bDefined = false;

						FOR_EACH_VEC( animDataList, aIt )
						{
							const FbxDmxAnimData_t *pFbxDmxAnimData = animDataList[aIt];
							if ( pFbxDmxAnimData->m_pDmeFloatLog && pFbxDmxAnimData->m_fbxProperty.IsValid() && !V_stricmp( pFbxDmxAnimData->m_fbxProperty.GetName(), pszChannelName ) )
							{
								if ( Verbose1() )
								{
									Warning( "     - vstFlexSlider Already defined FlexControl Channel: %s, ignoring FbxBlendShapeChannel with same name\n", pszChannelName );
								}
								bDefined = true;
								break;
							}
						}

						if ( !bDefined )
						{
							if ( Verbose1() )
							{
								Msg( "     - FbxBlendShapeChannel: %s\n", pszChannelName );
							}

							FbxDmxAnimData_t *pAnimData = animDataList[ animDataList.AddToTail( new FbxDmxAnimData_t ) ];
							animDataList.AddToTail( pAnimData );
							pAnimData->m_pFbxAnimCurve = pFbxAnimCurve;

							pAnimData->m_pDmeFloatLog = CreateMorphChannel( pszChannelName, pDmeChannelsClip );
						}
					}
				}
			}
		}
	}

	FbxDmxAnimData_t *pAnimData = new FbxDmxAnimData_t;
	animDataList.AddToTail( pAnimData );

	CDmeDag *pDmeDag = fbxToDmxMap.Element( nIndex );
	const bool bIsRoot = pDmeDag->GetValue( "__rootNode", false );
	pAnimData->m_pFbxNode = pFbxNode;
	pAnimData->m_pParent = bIsRoot ? NULL : pAnimDataParent;

	CDmeTransform *pDmeTransform = pDmeDag->GetTransform();

	CDmeChannel *pDmePosChannel = CreateElement< CDmeChannel >( CFmtStr( "%s_p", pDmeTransform->GetName() ).Access(), pDmeChannelsClip->GetFileId() );
	pDmePosChannel->SetMode( CM_PLAY );
	pDmePosChannel->SetOutput( pDmeTransform, "position" );
	CDmeVector3Log *pDmePosLog = pDmePosChannel->CreateLog< Vector >();
	pDmePosLog->SetValueThreshold( 1.0e-6 );
	pDmeChannelsClip->m_Channels.AddToTail( pDmePosChannel );

	CDmeChannel *pDmeRotChannel = CreateElement< CDmeChannel >( CFmtStr( "%s_o", pDmeTransform->GetName() ).Access(), pDmeChannelsClip->GetFileId() );
	pDmeRotChannel->SetMode( CM_PLAY );
	pDmeRotChannel->SetOutput( pDmeTransform, "orientation" );
	CDmeQuaternionLog *pDmeRotLog = pDmeRotChannel->CreateLog< Quaternion >();
	pDmeRotLog->SetValueThreshold( 1.0e-6 );
	pDmeChannelsClip->m_Channels.AddToTail( pDmeRotChannel );

	pAnimData->m_pDmePosLog = pDmePosLog;
	pAnimData->m_pDmeRotLog = pDmeRotLog;

	for ( int i = 0; i < pFbxNode->GetChildCount(); ++i )
	{
		ComputeAnimDataList_R( pFbxAnimLayer, animDataList, pDmeChannelsClip, fbxToDmxMap, pFbxNode->GetChild( i ), pAnimData );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmFbxSerializer::LoadAnimation(
	CDmElement *pDmeRoot, CDmeModel *pDmeModel, const FbxToDmxMap_t &fbxToDmxMap, FbxScene *pFbxScene, FbxNode *pFbxRootNode, FbxTime::EMode eFbxTimeMode ) const
{
	// Only process the first animation layer of the first animation stack
	FbxAnimStack *pFbxAnimStack = pFbxScene->GetSrcObject<FbxAnimStack>();
	if ( !pFbxAnimStack )
		return;

	FbxAnimLayer *pFbxAnimLayer = pFbxAnimStack->GetMember<FbxAnimLayer >();
	if ( !pFbxAnimLayer )
		return;

    FbxArray< FbxString * > mAnimStackNameArray;
	pFbxScene->FillAnimStackNameArray( mAnimStackNameArray );

	if ( mAnimStackNameArray.GetCount() <= 0 )
		return;
		
	pFbxScene->SetCurrentAnimationStack( pFbxAnimStack );

	FbxTakeInfo *pFbxTakeInfo = pFbxScene->GetTakeInfo( *( mAnimStackNameArray[0] ) );
	if ( !pFbxTakeInfo )
		return;

	const FbxTime tFbxStart = pFbxTakeInfo->mLocalTimeSpan.GetStart();
	const FbxLongLong nFbxStart = tFbxStart.GetFrameCount( eFbxTimeMode );
	const FbxTime tFbxEnd = pFbxTakeInfo->mLocalTimeSpan.GetStop();
	const FbxLongLong nFbxEnd = tFbxEnd.GetFrameCount( eFbxTimeMode );

	const double flFrameRate = FbxTime::GetFrameRate( eFbxTimeMode );
	const DmeFramerate_t dmeFrameRate( static_cast< float >( flFrameRate ) );

	if ( Verbose1() )
	{
		Msg( " * Animation Stack %s Layer %s - %s\n", pFbxAnimStack->GetName(), pFbxAnimLayer->GetName(), mAnimStackNameArray[0]->Buffer() );
		Msg( "   + Fbx Time Start:  %6.2f End: %6.2f Duration: %6.2f @ %6.2f fps\n",
			tFbxStart.GetSecondDouble(),
			tFbxEnd.GetSecondDouble(),
			pFbxTakeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble(),
			flFrameRate );
		Msg( "   + Fbx Frame Start: %6lld End: %6lld Duration: %6lld @ %6.2f fps\n",
			nFbxStart,
			nFbxEnd,
			pFbxTakeInfo->mLocalTimeSpan.GetDuration().GetFrameCount( eFbxTimeMode ),
			flFrameRate );
	}

	const char *pszAnimName = "anim";	// TODO: Get this value, name of file?

	DmFileId_t nFileId = pDmeRoot->GetFileId();

	CDmeAnimationList *pDmeAnimationList = CreateElement< CDmeAnimationList >( pszAnimName, nFileId );
	CDmeChannelsClip *pDmeChannelsClip = CreateElement< CDmeChannelsClip >( pszAnimName, nFileId );
	pDmeAnimationList->AddAnimation( pDmeChannelsClip );
	pDmeRoot->SetValue( "animationList", pDmeAnimationList );
	pDmeModel->SetValue( "animationList", pDmeAnimationList );

	pDmeChannelsClip->SetStartTime( DmeTime_t( tFbxStart.GetSecondDouble() ) );
	pDmeChannelsClip->SetTimeOffset( pDmeChannelsClip->GetStartTime() );

	pDmeChannelsClip->SetValue( "frameRate", static_cast< int >( dmeFrameRate.GetFramesPerSecond() ) );

	CUtlVector< FbxDmxAnimData_t * > animList;

	// Look for flex animation on vstFlexSlider nodes first
	for ( int i = 0; i < pFbxRootNode->GetChildCount(); ++i )
	{
		ComputeVstFlexSliderAnimDataList_R( pFbxAnimLayer, animList, pDmeChannelsClip, fbxToDmxMap, pFbxRootNode->GetChild( i ) );
	}

	for ( int i = 0; i < pFbxRootNode->GetChildCount(); ++i )
	{
		ComputeAnimDataList_R( pFbxAnimLayer, animList, pDmeChannelsClip, fbxToDmxMap, pFbxRootNode->GetChild( i ), NULL );
	}

	matrix3x4_t mDmeWorld;
	matrix3x4_t mDmeLocal;

	FbxLongLong nFbxCurrent;
	FbxTime tFbxCurrent;

	for ( nFbxCurrent = nFbxStart; nFbxCurrent <= nFbxEnd; nFbxCurrent += 1 )
	{
		tFbxCurrent.SetFrame( nFbxCurrent, eFbxTimeMode );
		const DmeTime_t tDmeCurrent( tFbxCurrent.GetSecondDouble() );

		if ( Verbose2() )
		{
			Msg( " * Fbx Time: %6.2f Fbx Frame: %6lld Dmx Time: %6.2f Dmx Frame: %d\n",
				tFbxCurrent.GetSecondDouble(), tFbxCurrent.GetFrameCount( eFbxTimeMode ),
				tDmeCurrent.GetSeconds(), tDmeCurrent.CurrentFrame( dmeFrameRate ) );
		}

		for ( int i = 0; i < animList.Count(); ++i )
		{
			FbxDmxAnimData_t *pAnimData = animList[ i ];

			if ( pAnimData->m_pDmeFloatLog )
			{
				if ( pAnimData->m_fbxProperty.IsValid() )
				{
					float flFloatValue = pAnimData->m_fbxProperty.EvaluateValue< float >( tFbxCurrent );
					if ( pAnimData->m_bNormalize )
					{
						flFloatValue = RemapVal( flFloatValue, pAnimData->m_flMin, pAnimData->m_flMax, 0.0f, 1.0f );
					}
					pAnimData->m_pDmeFloatLog->SetKey( tDmeCurrent, flFloatValue );
				}
				else if ( pAnimData->m_pFbxAnimCurve )
				{
					const float flFloatValue = pAnimData->m_pFbxAnimCurve->Evaluate( tFbxCurrent );
					pAnimData->m_pDmeFloatLog->SetKey( tDmeCurrent, flFloatValue );
				}

				continue;
			}

			FbxNode *pFbxNode = pAnimData->m_pFbxNode;

			const FbxAMatrix &mFbxWorld = pFbxNode->EvaluateGlobalTransform( tFbxCurrent );

			const FbxVector4 vFbxTranslate = mFbxWorld.GetT();
			const FbxQuaternion qFbxRotate = mFbxWorld.GetQ();

			Assert( vFbxTranslate[3] == 0.0 || vFbxTranslate[3] == 1.0 );

			Vector vDmeTranslate( vFbxTranslate[0], vFbxTranslate[1], vFbxTranslate[2] );
			Quaternion qDmeRotate( qFbxRotate[0], qFbxRotate[1], qFbxRotate[2], qFbxRotate[3] );

			AngleMatrix( RadianEuler( qDmeRotate ), vDmeTranslate, mDmeWorld );
			pAnimData->SetWorldMatrix( mDmeWorld );

			if ( pAnimData->m_pParent )
			{
				MatrixMultiply( pAnimData->m_pParent->m_mWorldInverse, mDmeWorld, mDmeLocal );
				MatrixAngles( mDmeLocal, qDmeRotate, vDmeTranslate );
			}

			pAnimData->m_pDmePosLog->SetKey( tDmeCurrent, vDmeTranslate );
			pAnimData->m_pDmeRotLog->SetKey( tDmeCurrent, qDmeRotate );
		}
	}

	pDmeChannelsClip->SetDuration( DmeTime_t( pFbxTakeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble() ) );

	if ( Verbose1() )
	{
		Msg( "   + Dme Time Start:  %6.2f End: %6.2f Duration: %6.2f @ %6.2f fps\n",
			pDmeChannelsClip->GetStartTime().GetSeconds(),
			pDmeChannelsClip->GetEndTime().GetSeconds(),
			pDmeChannelsClip->GetDuration().GetSeconds(),
			flFrameRate );
		Msg( "   + Dme Frame Start: %6d End: %6d Duration: %6d @ %6.2f fps\n",
			pDmeChannelsClip->GetStartTime().CurrentFrame( dmeFrameRate ),
			pDmeChannelsClip->GetEndTime().CurrentFrame( dmeFrameRate ),
			pDmeChannelsClip->GetDuration().CurrentFrame( dmeFrameRate ),
			flFrameRate );
	}

	animList.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
FbxManager *CDmFbxSerializer::GetFbxManager()
{
	static bool bWarned = false;

	if ( !g_pFbx )
	{
		if ( !bWarned )
		{
			Log_Warning( LOG_FBX_SYSTEM, "Warning! FBX system not initialized\n" );
			bWarned = true;
		}

		return NULL;
	}

	return g_pFbx->GetFbxManager();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDmFbxSerializer::AddConversionError( DmFileId_t nDmFileId, const char *pszErrorMsg )
{
	if ( !pszErrorMsg )
		return;

	CDmElement *pDmRoot = g_pDataModel->GetElement( g_pDataModel->GetFileRoot( nDmFileId ) );

	if ( !pDmRoot )
		return;

	CDmAttribute *pConversionErrorsAttr = pDmRoot->AddAttribute( "conversionErrors", AT_STRING_ARRAY );
	if ( pConversionErrorsAttr )
	{
		CDmrStringArray conversionErrors( pConversionErrorsAttr );
		for ( int i = 0; i < conversionErrors.Count(); ++i )
		{
			if ( !V_stricmp( conversionErrors[i], pszErrorMsg ) )
				return;
		}

		conversionErrors.AddToTail( pszErrorMsg );
		Warning( "%s\n", pszErrorMsg );
	}
}
