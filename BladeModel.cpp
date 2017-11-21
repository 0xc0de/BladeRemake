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

#include "BladeModel.h"
#include "BladeWorld.h"

#include "FileDump.h"

#include <Framework/IO/Public/FileUrl.h>
#include <Engine/Resource/Public/ResourceManager.h>

#include <map>

#pragma warning( disable : 4189 )

void FBladeModel::LoadModel( const char * _FileName ) {
    struct FVertex {
        FDVec3 Position;
        FDVec3 Normal;
    };

    struct FPolygon {
        int Indices[3];
        FVec2 TexCoords[3];
        int Unknown;
    };

    TArrayList< FMeshVertex > MeshVertices;
    TMutableArray< unsigned int > MeshIndices;
    TArrayList< FMeshOffset > MeshOffsets;
    FMeshVertex Vertex;
    FString TextureName;
    TArrayList< FVertex > Vertices;
    TArrayList< FPolygon > Polygons;

    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    SetDumpLog( false );

    FString Name = DumpString( File );

    int VerticesCount = DumpInt( File );
    Vertices.Resize( VerticesCount );
    for ( int i = 0 ; i < VerticesCount ; i++ ) {
        FVertex & v = Vertices[ i ];

        v.Position.x = DumpDouble( File );
        v.Position.y = DumpDouble( File );
        v.Position.z = DumpDouble( File );

        v.Position *= BLADE_COORD_SCALE;

        v.Normal.x = DumpDouble( File );
        v.Normal.y = -DumpDouble( File );
        v.Normal.z = -DumpDouble( File );
    }

    memset( &Vertex, 0, sizeof( Vertex ) );

    std::map< std::string, TMutableArray< int > > TextureMap;

    Out() << "Texture Unknown Ints:";

    int NumTextures = DumpInt( File );
    Polygons.Resize( NumTextures );

    for ( int i = 0 ; i < NumTextures ; i++ ) {

        FPolygon & Polygon = Polygons[i];

        Polygon.Indices[0] = DumpInt( File );
        Polygon.Indices[1] = DumpInt( File );
        Polygon.Indices[2] = DumpInt( File );

        TextureName = DumpString( File );

        TextureMap[ TextureName.Str() ].Append( i );
    
        Polygon.TexCoords[0].x = DumpFloat( File );
        Polygon.TexCoords[1].x = DumpFloat( File );
        Polygon.TexCoords[2].x = DumpFloat( File );
        Polygon.TexCoords[0].y = DumpFloat( File );
        Polygon.TexCoords[1].y = DumpFloat( File );
        Polygon.TexCoords[2].y = DumpFloat( File );

        SetDumpLog( true );
        Polygon.Unknown = DumpInt( File );
        SetDumpLog( false );
    }

    MeshOffsets.Resize( TextureMap.size() );

    int OffsetIndex = 0;
    int StartIndexLocation = 0;
    for ( auto It : TextureMap ) {

        const TMutableArray< int > & PolygonIndices = It.second;

        for ( int i = 0 ; i < PolygonIndices.Length() ; i++ ) {
            for ( int j = 0 ; j < 3 ; j++ ) {
                FPolygon & Polygon = Polygons[ PolygonIndices[i] ];

                FVertex & v = Vertices[ Polygon.Indices[j] ];
                Vertex.Position.x = v.Position.x;
                Vertex.Position.y = v.Position.y;
                Vertex.Position.z = v.Position.z;
                Vertex.Normal.x = v.Normal.x;
                Vertex.Normal.y = v.Normal.y;
                Vertex.Normal.z = v.Normal.z;
                Vertex.TexCoord = Polygon.TexCoords[j];

                MeshIndices.Append( MeshVertices.Length() );
                MeshVertices.Append( Vertex );
            }
        }

        FMeshOffset & Offset = MeshOffsets[OffsetIndex];
        Offset.IndexCount = PolygonIndices.Length() * 3;
        Offset.StartIndexLocation = StartIndexLocation;
        Offset.Abstract = It.first.c_str();

        ++OffsetIndex;

        StartIndexLocation += Offset.IndexCount;
    }

    int PartsCount = DumpInt( File );

    Parts.Resize( PartsCount );

    uint32 StrLen = DumpIntNotSeek( File );
    if ( StrLen != 0xffffffff ) {

        SetDumpLog( true );

        Out() << "Parts";

        for ( int n = 0 ; n < PartsCount  ; n++ ) {
            FPart & Part = Parts[ n ];

            Part.Name = DumpString( File );

            Out() << "Part" << Part.Name;

            Part.UnknownIndex = DumpInt( File );

            SetDumpLog( false );
            for ( int i = 0 ; i < 16 ; i++ ) {
                FMath::ToPtr( Part.Matrix )[i] = DumpDouble( File );
            }
            SetDumpLog( true );

            DumpInt( File );
            DumpInt( File );

            int Count = DumpInt( File );

            for ( int i = 0 ; i < Count ; i++ ) {
                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );
                DumpInt( File );
                DumpInt( File );
            }
        }

        Out() << "Unknown doubles";
        UnknownDbl0 = DumpDouble( File );
        UnknownDbl1 = DumpDouble( File );
        UnknownDbl2 = DumpDouble( File );
        UnknownDbl3 = DumpDouble( File );

        Out() << "Unknown ints";

        int UnknownI0 = DumpInt( File );

        assert( UnknownI0 == 0 ); // FIXME

        int UnknownI1 = DumpInt( File );

        assert( UnknownI1 == 0 ); // FIXME

        SetDumpLog( false );

        int SocketsCount = DumpInt( File );

        Sockets.Resize( SocketsCount );

        Out() << "Sockets";

        for ( int n = 0 ; n < SocketsCount ; n++ ) {
            FSocket & Socket = Sockets[ n ];

            Socket.Name = DumpString( File );

            for ( int i = 0 ; i < 16 ; i++ ) {
                FMath::ToPtr( Socket.Matrix )[i] = DumpDouble( File );
            }

            SetDumpLog( true );
            Socket.UnknownIndex = DumpInt( File );
            SetDumpLog( false );
        }

        SetDumpLog( true );

        Out() << "Unknown ints";
        // 4
        int UnknownI2 = DumpInt( File );  // FIXME
        // 0, 8
        int Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpInt( File );
            DumpInt( File );
            for ( int i = 0 ; i < 9 ; i++ ) {
                DumpDouble( File );
            }
        }

        // 0, 2
        Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpInt( File );
            DumpInt( File );
            for ( int i = 0 ; i < 6 ; i++ ) {
                DumpDouble( File );
            }
        }

        Out() << "----------";
        Count = DumpInt( File );

        for ( int n = 0 ; n < Count ; n++ ) {
            DumpByte( File );
        }

        Out() <<"------------";

        Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpInt( File );
        }

        int UnknownI5 = DumpInt( File );
        assert( UnknownI5 == 0 );

        Out() <<"------------";

    } else {

        assert( PartsCount == 1 );

        FPart & Part = Parts[ 0 ];

        File->Seek( 4, FFileAbstract::SeekCur );

        SetDumpLog( true );
        DumpFileOffset( File );
        SetDumpLog( false );

        Part.UnknownIndex = 0;

        // Matrix 4x4
        for ( int i = 0 ; i < 16 ; i++ ) {
            FMath::ToPtr( Part.Matrix )[ i ] = DumpDouble( File );
        }
        //Out() << "Matrix:" << Part.Matrix;

        SetDumpLog( true );

        DumpInt( File );
        DumpInt( File );

        int Count = DumpInt( File );

        for ( int i = 0 ; i < Count ; i++ ) {
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );

            DumpInt( File );
            DumpInt( File );
        }

        Out() << "Unknown doubles";
        UnknownDbl0 = DumpDouble( File );
        UnknownDbl1 = DumpDouble( File );
        UnknownDbl2 = DumpDouble( File );
        UnknownDbl3 = DumpDouble( File );

        Out() << "Unknown ints";

        // 0
        int UnknownI0 = DumpInt( File ); // FIXME

        Count = DumpInt( File );

        for ( int i = 0 ; i < Count ; i++ ) {
            Out() << "----";
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );

            DumpInt( File );
        }

        Out() << "Unknown ints";

        if ( DumpIntNotSeek( File ) == 0xffffffff ) {
            // ffffffff
            DumpInt( File );
            // 0
            DumpInt( File );
            // 1
            DumpInt( File );
            DumpFloat( File );
            DumpFloat( File );
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );
            // ffffffff
            DumpInt( File );
        }

        Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpString( File );
            for ( int i = 0 ; i < 16 ; i++ ) {
                DumpDouble( File );
            }
            DumpInt( File );
        }

        // 4
        int UnknownI3 = DumpInt( File );
        if ( UnknownI3 == 4 ) { // FIXME
            // 0
            Count = DumpInt( File );
            for ( int n = 0 ; n < Count ; n++ ) {
                for ( int i = 0 ; i < 10 ; i++ ) {
                    DumpDouble( File );
                }
            }

            // 0
            Count = DumpInt( File );
            for ( int n = 0 ; n < Count * 7 ; n++ ) {
                DumpDouble( File );
            }

            Out() << "----------";

            Count = DumpInt( File );
            for ( int n = 0 ; n < Count ; n++ ) {
                DumpByte( File );
            }

            Count = DumpInt( File );
            assert( Count == 0 );

            Count = DumpInt( File );
            for ( int n = 0 ; n < Count * 7 ; n++ ) {
                DumpDouble( File );
            }

        } else { // UnknownI2 == 2

            // trono.BOD
            // firybally.BOD
            // leon.BOD
            // reyaure.BOD

            int Unk = DumpInt( File );
            assert( Unk == 0 );

            Unk = DumpInt( File );
            assert( Unk == 0 );

            Count = DumpInt( File );
            for ( int i = 0 ; i < Count ; i++ ) { // FIXME
                DumpByte( File );
            }
        }
    }

    // Check eof
    byte b;
    assert( 0 == File->ReadByte( b ) );

    FFiles::CloseFile( File );

    Resource = GResourceManager->GetResource< FStaticMeshResource >( Name.Str() );
    CalcTangentSpace( MeshVertices.ToPtr(), MeshVertices.Length(), MeshIndices.ToPtr(), MeshIndices.Length() );
    Resource->SetVertexData( MeshVertices.ToPtr(), MeshVertices.Length(), MeshIndices.ToPtr(), MeshIndices.Length(), false );
    Resource->SetMeshOffsets( MeshOffsets.ToPtr(), MeshOffsets.Length() );
}