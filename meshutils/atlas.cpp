//=========== Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Mesh class UV parameterization operations.
//
//===========================================================================//
#include "mesh.h"

class CPackNode
{
public:
	CPackNode*			m_child[2];
	Rect_t				m_rect;
	AtlasChart_t*		m_pChart;

	float				m_flTotalW;
	float				m_flTotalH;

public:
	CPackNode( Rect_t rect, float flTotalW, float flTotalH );
	~CPackNode();

	CPackNode *InsertChart( AtlasChart_t* pTexture ); 
};

class CAtlasPacker
{
private:
	int					m_nWidth;
	int					m_nHeight;

	CPackNode			*m_pRootNode;

public:
	CAtlasPacker();
	~CAtlasPacker();

	void Init( int nWidth, int nHeight );

	bool InsertChart( AtlasChart_t *pTexture );
};

int SortAtlasCharts( AtlasChart_t* const *pOne, AtlasChart_t* const *pTwo )
{
	int nSizeOne = MAX( (*pOne)->m_vMaxTextureSize.x, (*pOne)->m_vMaxTextureSize.y );
	int nSizeTwo = MAX( (*pTwo)->m_vMaxTextureSize.x, (*pTwo)->m_vMaxTextureSize.y );

	if ( nSizeOne < nSizeTwo )
		return -1;
	else if ( nSizeOne > nSizeTwo )
		return 1;
	return 0;
}

//--------------------------------------------------------------------------------------
// Pack charts into an atlas.  If nAtlasGrow is non-zero, we will attempt to create an atlas
// starting at nAtlasTextureSizeX and growing by nAtlasGrow every time until we eventually
// get an atlas.  If nAtlasGrow is 0, then we return the number of charts that didn't
// get atlased.
//--------------------------------------------------------------------------------------
int PackChartsIntoAtlas( AtlasChart_t *pCharts, int nCharts, int nAtlasTextureSizeX, int nAtlasTextureSizeY, int nAtlasGrow )
{
	// Create a duplicate vector to sort so that the input remains in the same order
	CUtlVector<AtlasChart_t*> chartVector;
	CUtlVector<bool> chartUsed;
	chartVector.EnsureCount( nCharts );
	chartUsed.EnsureCount( nCharts );

	for ( int c=0; c<nCharts; ++c )
	{
		chartVector[ c ] = &pCharts[ c ];
		chartUsed[ c ] = false;
	}
	chartVector.Sort( SortAtlasCharts );

	// Try to get the most out of our texture space
	bool bTryGrow = ( nAtlasGrow > 0 );
	bool bHaveAtlas = false;
	int nAtlasSizeX = nAtlasTextureSizeX;
	int nAtlasSizeY = nAtlasTextureSizeY;
	int nAttempt = 0;
	int nUnatlased = 0;

	while ( !bHaveAtlas )
	{
		Msg( "Atlas Attempt: %d\n", nAttempt );
		// assume we have an atlas
		bHaveAtlas = true;

		// increment and try again
		nAtlasSizeX += nAtlasGrow;
		nAtlasSizeY += nAtlasGrow;

		CAtlasPacker m_packer;
		m_packer.Init( nAtlasSizeX, nAtlasSizeY );

		// insert largest first
		for ( int t=nCharts-1; t>=0; --t )
		{
			AtlasChart_t *pChart = chartVector[ t ];

			if ( !pChart->m_bAtlased )
			{
				if ( m_packer.InsertChart( pChart ) )
				{
					pChart->m_bAtlased = true;
				}
				else
				{
					if ( bTryGrow )
					{
						bHaveAtlas = false;
						nAttempt++;
						break;
					}
					else
					{
						nUnatlased++;
					}
				}
			}
		}
	}

	return nUnatlased;
}

//CPackNode
CPackNode::CPackNode( Rect_t rect, float flTotalW, float flTotalH ) :
	m_pChart( NULL ),
	m_flTotalW( flTotalW ),
	m_flTotalH( flTotalH )
{
	m_child[0] = NULL;
	m_child[1] = NULL;
	m_rect = rect;
}

CPackNode::~CPackNode()
{
	if ( m_child[ 0 ] ) { delete m_child[ 0 ]; }
	if ( m_child[ 1 ] ) { delete m_child[ 1 ]; }
}

CPackNode* CPackNode::InsertChart( AtlasChart_t *pChart )
{
	int texWidth = (int)ceil( pChart->m_vMaxTextureSize.x );
	int texHeight = (int)ceil( pChart->m_vMaxTextureSize.y );

	//if we have children, that means we can't insert into this node, try the kids
	if ( NULL != m_child[ 0 ] && NULL != m_child[ 1 ] )
	{
		//try the first child
		CPackNode* pNewNode = m_child[ 0 ]->InsertChart( pChart );
		if(pNewNode)
			return pNewNode;

		//if that didn't work, try the second
		pNewNode = m_child[ 1 ]->InsertChart( pChart );
		return pNewNode;	//if this didn't work it will return NULL
	}
	else	//else, see if we can fit it in
	{
		//if we are a leaf of the tree (m_child[0] and m_child[1] have textures in them,
		//then make sure we don't have texture already in here
		if ( m_pChart )
			return NULL;	//if we already have a texture, return NULL

		//else, see if we can even fit the lightmap
		int width = m_rect.width;// + 1;
		int height = m_rect.height;// + 1;

		if ( width < texWidth || height < texHeight )
			return NULL;		//we don't fit!!!

		//if we're just the right size, then add the lightmap and we're done
		if ( width == texWidth && height == texHeight )
		{
			m_pChart = pChart;		//mark this as the texture for the current node

			//get the new texture coordinates and put them in the texture
			{
				m_pChart->m_vAtlasMin.x = m_rect.x / m_flTotalW;
				m_pChart->m_vAtlasMin.y = m_rect.y / m_flTotalH;

				m_pChart->m_vAtlasMax.x = ( m_rect.x + m_rect.width ) / m_flTotalW;
				m_pChart->m_vAtlasMax.y = ( m_rect.y + m_rect.height ) / m_flTotalH;
			}

			return this;	//return us, since we're the right size
		}

		//if we're not the right size, but we're big enough to hold the lightmap,
		//split us up into two nodes
		Rect_t rect0,rect1;

		int dw = width - texWidth;
		int dh = height - texHeight;

		if( dw > dh )	//split left, right
		{
			//left rect
			rect0.x = m_rect.x;
			rect0.width = texWidth;// - 1;
			rect0.y = m_rect.y;
			rect0.height = m_rect.height;
			//right rect
			rect1.x = m_rect.x + rect0.width;
			rect1.width = m_rect.width - rect0.width;
			rect1.y = m_rect.y;
			rect1.height = m_rect.height;
		}
		else			//split up, down
		{
			//top rect
			rect0.x = m_rect.x;
			rect0.width = m_rect.width;
			rect0.y = m_rect.y;
			rect0.height = texHeight;// - 1;
			//bottom rect
			rect1.x = m_rect.x;
			rect1.width = m_rect.width;
			rect1.y = m_rect.y + rect0.height;
			rect1.height = m_rect.height - rect0.height;
		}
		
		m_child[ 0 ] = new CPackNode( rect0, m_flTotalW, m_flTotalH );
		m_child[ 1 ] = new CPackNode( rect1, m_flTotalW, m_flTotalH );

		//since we made the first child the size we needed, insert into him.
		//this should never fail
		return m_child[ 0 ]->InsertChart( pChart );
	}

}

//clightmappacker class
CAtlasPacker::CAtlasPacker()
{
	m_nWidth = 0;
	m_nHeight = 0;

	m_pRootNode = NULL;
}

CAtlasPacker::~CAtlasPacker()
{
	if ( m_pRootNode ) delete m_pRootNode;
}

void CAtlasPacker::Init( int nWidth, int nHeight )
{
	m_nWidth = nWidth;
	m_nHeight = nHeight;

	Rect_t rect;
	rect.x = 0; rect.width = nWidth;
	rect.y = 0; rect.height = nHeight;
	m_pRootNode = new CPackNode( rect, (float)nWidth, (float)nHeight );
}

bool CAtlasPacker::InsertChart( AtlasChart_t *pChart )
{
	if( !m_pRootNode )
		return false;

	CPackNode* pNode = m_pRootNode->InsertChart( pChart );

	if(pNode)
	{
		return true;
	}

	return false;
}