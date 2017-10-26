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

#include "BladeCAM.h"
#include "FileDump.h"

#include <Framework/IO/Public/FileUrl.h>

void FCameraRecord::LoadRecord( const char * _FileName ) {
    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    SetDumpLog( true );

    int32 LastFrameIndex;
    File->ReadSwapInt32( LastFrameIndex );

    Frames.Resize( LastFrameIndex + 1 );

    FVec3 Axis;
    float Angle;

    UnknownFloat = DumpFloat( File ); // what is it?

    for ( int i = 0 ; i < Frames.Length() ; i++ ) {
        FFrame & Frame = Frames[i];

        Out() << "------------- Frame" << i << "------------------";

        File->ReadSwapVector( Axis );
        Axis.y = -Axis.y;
        Axis.z = -Axis.z;

        File->ReadSwapFloat( Angle );
        
        FMat3 m = FMath::Transpose( FMath::RotateAxisNormalizedMat3x3( FMath::_PI/2.0f, FVec3(1,0,0) ) * FMath::RotateMat3x3( Angle, Axis ) );
        Frame.Rotation = FMath::ToQuat( m );

        File->ReadSwapVector( Frame.Position );
        Frame.Position.x *= 0.001f;
        Frame.Position.y *= -0.001f;
        Frame.Position.z *= -0.001f;

        Frame.TimeScale = /*1.0f / */DumpFloat( File ); // what is it?
    }

    Out() << "------------------------------------------------";

    FFiles::CloseFile( File );
}
