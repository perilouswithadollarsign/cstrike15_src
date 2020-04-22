//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if !defined( SFUIMEMORYFILE_H_ )
#define SFUIMEMORYFILE_H_

class SFUIMemoryFile : public SF::File
{
public:

    const char* GetFilePath( void )       { return m_filePath.ToCStr(); }

    bool        IsValid( void )           { return m_valid; }
    bool        IsWritable( void )        { return false; }

    bool        Flush( void )             { return true; }
    int		    GetErrorCode( void )      { return 0; }

    int		    Tell( void )              { return m_fileIndex; }
    SF::SInt64  LTell( void )             { return (SF::SInt64) m_fileIndex; }

    int		    GetLength( void )         { return m_fileSize; }
    SF::SInt64  LGetLength( void )        { return (SF::SInt64) m_fileSize; }

    bool Close( void )
	{
		m_valid = false;
		return false;
	}

	int CopyFromStream( SF::File *pstream, int byteSize )
	{
		return 0;
	}

	int Write( const SF::UByte *pbuffer, int numBytes )
	{
		return 0;
	}

	int Read( SF::UByte *pbufer, int numBytes )
	{
		if ( m_fileIndex + numBytes > m_fileSize )
		{
			numBytes = m_fileSize - m_fileIndex;
		}

		if ( numBytes > 0 )
		{
			V_memcpy( pbufer, &m_fileData[m_fileIndex], numBytes );

			m_fileIndex += numBytes;
		}

		return numBytes;
	}

	int SkipBytes( int numBytes )
	{
		if ( m_fileIndex + numBytes > m_fileSize )
		{
			numBytes = m_fileSize - m_fileIndex;
		}

		m_fileIndex += numBytes;

		return numBytes;
	}

	int BytesAvailable( void )
	{
		return ( m_fileSize - m_fileIndex );
	}

	int Seek( int offset, int origin = Seek_Set )
	{
		switch ( origin )
		{
			case Seek_Set:
				m_fileIndex = offset;
				break;
			case Seek_Cur:
				m_fileIndex += offset;
				break;
			case Seek_End:
				m_fileIndex = m_fileSize - offset;
				break;
			default:
				break;
		}

		return m_fileIndex;
	}

	SF::SInt64 LSeek( SF::SInt64 offset, int origin = Seek_Set )
	{
		return ( SF::SInt64 ) Seek( ( int ) offset, origin );
	}

	bool ChangeSize( int newSize )
	{
		if ( newSize <= m_buffer.Size() )
		{
			m_fileSize = newSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	CUtlBuffer& GetBuffer( void )
	{
		return m_buffer;
	}

public:

	// pfileName should be encoded as UTF-8 to support international file names.
	SFUIMemoryFile( const char* pfileName ) :
		m_filePath( pfileName )
	{
	}

	void Init( void )
	{
		m_fileData = ( const SF::UByte * ) m_buffer.Base();
		m_fileSize = m_buffer.Size();
		m_fileIndex = 0;
		m_valid = ( !m_filePath.IsEmpty() && m_fileData != NULL && m_fileSize > 0 ) ? true : false;
	}

private:

	SF::String m_filePath;
	CUtlBuffer m_buffer;

	const SF::UByte *m_fileData;
	SF::SInt32 m_fileSize;
	SF::SInt32 m_fileIndex;
	bool m_valid;
};



#endif /* SFUIMEMORYFILE_H_ */
