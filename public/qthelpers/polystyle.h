#pragma once

#include <QtGui/QtGui>
#include "tier0/platform.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

class CQPolyStyle
{
public:
	CQPolyStyle();
	CQPolyStyle( QString fileName );
	void LoadFile( QString fileName );
	void AddToken( QString tokenName, QString tokenValue );
	QString GetFinalStyle();
	QString ProcessString( QString str );
private:
	void IncludeFile( QString fileName );
	QMap<QString,QString> m_SymbolTable;
	QString m_FinalStyle;
};

inline QString PolyStyle( QString fileName )
{
	CQPolyStyle s( fileName );
	return s.GetFinalStyle();
}
