/*

Blade Of Darkness Remake GPL Source Code

Copyright (C) 2017 Alexander Samusev.

This file is part of the Blade Of Darkness Remake GPL Source Code (BladeRemake Source Code).  

BladeRemake is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "FileDump.h"

static bool DumpLogEnabled = false;

void SetDumpLog( bool _Enable ) {
    DumpLogEnabled = _Enable;
}

void DumpUnknownBytes( FFileAbstract * _File, int _BytesCount ) {
    byte * Bytes = new byte[ _BytesCount ];
    long Position = _File->Tell();
    _File->Read( Bytes, _BytesCount );
    if ( DumpLogEnabled ) {
        for ( byte * pByte = Bytes ; pByte < &Bytes[_BytesCount] ; pByte++ ) {
            Out() << FMath::ToHexString( Position++, true ) << ":" << FMath::ToHexString( *pByte, true ) << char( *pByte );
        }
    }
    delete [] Bytes;
}

void DumpIntOrFloat( FFileAbstract * _File ) {
    int32 Unknown;
    byte * pUnknown = (byte *)&Unknown;
    _File->ReadSwapInt32( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 4, true ) << ":" << Unknown << *(float *)&Unknown << "hex :" << FMath::ToHexString( pUnknown[0], true ) << FMath::ToHexString( pUnknown[1], true ) << FMath::ToHexString( pUnknown[2], true ) << FMath::ToHexString( pUnknown[3], true );
    }
}

int32 DumpInt( FFileAbstract * _File ) {
    int32 Unknown;
    byte * pUnknown = (byte *)&Unknown;
    _File->ReadSwapInt32( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 4, true ) << ":" << Unknown << "hex :" << FMath::ToHexString( pUnknown[0], true ) << FMath::ToHexString( pUnknown[1], true ) << FMath::ToHexString( pUnknown[2], true ) << FMath::ToHexString( pUnknown[3], true );
    }
    return Unknown;
}

int16 DumpShort( FFileAbstract * _File ) {
    int16 Unknown;
    byte * pUnknown = (byte *)&Unknown;
    _File->ReadSwapInt16( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 2, true ) << ":" << Unknown << "hex :" << FMath::ToHexString( pUnknown[0], true ) << FMath::ToHexString( pUnknown[1], true );
    }
    return Unknown;
}

int32 DumpIntNotSeek( FFileAbstract * _File ) {
    int32 Unknown;
    byte * pUnknown = (byte *)&Unknown;
    _File->ReadSwapInt32( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 4, true ) << ":" << Unknown << "hex :" << FMath::ToHexString( pUnknown[0], true ) << FMath::ToHexString( pUnknown[1], true ) << FMath::ToHexString( pUnknown[2], true ) << FMath::ToHexString( pUnknown[3], true );
    }
    _File->Seek( -4, FFileAbstract::SeekCur );
    return Unknown;
}

int32 DumpByte( FFileAbstract * _File ) {
    byte Unknown;
    _File->Read( &Unknown, 1 );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 1, true ) << ":" << Unknown << "hex :" << FMath::ToHexString( Unknown, true );
    }
    return Unknown;
}

float DumpFloat( FFileAbstract * _File ) {
    float Unknown;
    byte * pUnknown = (byte *)&Unknown;
    _File->ReadSwapFloat( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 4, true ) << ":" << Unknown << "hex :" << FMath::ToHexString( pUnknown[0], true ) << FMath::ToHexString( pUnknown[1], true ) << FMath::ToHexString( pUnknown[2], true ) << FMath::ToHexString( pUnknown[3], true );
    }
    return Unknown;
}

double DumpDouble( FFileAbstract * _File ) {
    double Unknown;
    byte * pUnknown = (byte *)&Unknown;
    _File->ReadSwapDouble( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( _File->Tell() - 8, true ) << ":" << Unknown << "hex :" << FMath::ToHexString( pUnknown[0], true ) << FMath::ToHexString( pUnknown[1], true ) << FMath::ToHexString( pUnknown[2], true ) << FMath::ToHexString( pUnknown[3], true ) << FMath::ToHexString( pUnknown[4], true ) << FMath::ToHexString( pUnknown[5], true ) << FMath::ToHexString( pUnknown[6], true ) << FMath::ToHexString( pUnknown[7], true );
    }
    return Unknown;
}

FString DumpString( FFileAbstract * _File ) {
    FString Unknown;
    int FileOffset = _File->Tell();
    _File->ReadString( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FMath::ToHexString( FileOffset, true ) << ":" << Unknown;
    }
    return Unknown;
}

int DumpFileOffset( FFileAbstract * _File ) {
    int FileOffset = _File->Tell();
    if ( DumpLogEnabled ) {
        Out() << "FileOffset :" << FMath::ToHexString( FileOffset, true );
    }
    return FileOffset;
}
