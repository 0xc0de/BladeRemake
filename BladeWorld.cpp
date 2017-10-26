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

#include "BladeWorld.h"
#include "FileDump.h"

#include <Framework/Geometry/Public/PolygonClipper.h>
#include <Framework/IO/Public/FileUrl.h>

FBladeWorld World;

static const double BLADE_COORD_SCALE = 1;//0.001;

template<>
void TTriangulatorHelper_ContourVertexPosition( FDVec3 & _Dst, const FDVec2 & _Src ) {
    _Dst.x = _Src.x;
    _Dst.y = _Src.y;
    _Dst.z = 0;
}

template<>
void TTriangulatorHelper_TriangleVertexPosition( FDVec3 & _Dst, const FDVec2 & _Src ) {
    _Dst.x = _Src.x;
    _Dst.y = _Src.y;
    _Dst.z = 0;
}

template<>
void TTriangulatorHelper_CombineVertex( FDVec2 & _OutputVertex,
                                        const FDVec3 & _Position,
                                        const float * _Weights,
                                        const FDVec2 & _V0,
                                        const FDVec2 & _V1,
                                        const FDVec2 & _V2,
                                        const FDVec2 & _V3 ) {
    _OutputVertex.x = _Position.x;
    _OutputVertex.y = _Position.y;
}

template<>
void TTriangulatorHelper_CopyVertex( FDVec2 & _Dst, const FDVec2 & _Src ) {
    _Dst = _Src;
}

#if 0
static void PrintContour( const FClipperContour & _Contour ) {
    for ( int i = 0 ; i < _Contour.Length() ; i++ ) {
        Out() << _Contour[i];
    }
}

static void PrintPolygon( const FClipperPolygon & _Polygon ) {
    Out() << "OUTER:";
    PrintContour( _Polygon.Outer );

    if ( _Polygon.Outer.Length() == 0 ) {
        Out() << "EMPTY!!!";
    }

    for ( int i = 0 ; i < _Polygon.Holes.Length() ; i++ ) {
        Out() << "HOLE:";
        PrintContour( _Polygon.Holes[i] );
    }
}

static void PrintPolygons( const TArrayList< FClipperPolygon > & _Polygons ) {
    for ( int i = 0 ; i < _Polygons.Length() ; i++ ) {
        Out() << "--------- Poly" << i << "----------";
        PrintPolygon( _Polygons[i] );
    }
}
#endif

FBladeWorld::~FBladeWorld() {
    FreeWorld();
}

void FBladeWorld::LoadWorld( const char * _FileName ) {
    File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );

    if ( !File ) {
        return;
    }

    FreeWorld();

    int32 AtmospheresCount;
    File->ReadSwapInt32( AtmospheresCount );
    Atmospheres.Resize( AtmospheresCount );
    for ( int i = 0 ; i < AtmospheresCount ; i++ ) {
        FBladeMap::FAtmosphereEntry & Atmo = Atmospheres[ i ];
        File->ReadString( Atmo.Name );
        File->Read( &Atmo.Color[ 0 ], 3 );
        File->ReadSwapFloat( Atmo.Intensity );
    }

    int32 VerticesCount;
    File->ReadSwapInt32( VerticesCount );
    Vertices.Resize( VerticesCount );
    for ( int i = 0 ; i < VerticesCount ; i++ ) {
        File->ReadSwapDouble( Vertices[i].x );
        File->ReadSwapDouble( Vertices[i].y );
        File->ReadSwapDouble( Vertices[i].z );

        // Transform coordinates to our system
        //Vertices[i].y = /*-*/Vertices[i].y;
        //Vertices[i].z = /*-*/Vertices[i].z;
    }

    int32 SectorsCount;
    File->ReadSwapInt32( SectorsCount );
    Sectors.Resize( SectorsCount );

    for ( int SectorIndex = 0 ; SectorIndex < Sectors.Length() ; SectorIndex++ ) {
        FSector & Sector = Sectors[ SectorIndex ];

        SetDumpLog( true );

        DumpFileOffset( File );

        FString UnknownName;
        File->ReadString( UnknownName );

        Out() << "Loading sector:" << UnknownName;

        File->Read( &Sector.AmbientColor[ 0 ], 3 );
        File->ReadSwapFloat( Sector.AmbientIntensity );        

        SetDumpLog( false );

        DumpFloat( File );

        for ( int i = 0 ; i < 24 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        for ( int i = 0 ; i < 8 ; i++ ) {
            if ( DumpByte( File ) != 0xCD ) {
                Out() << "not CD";
            }
        }

        for ( int i = 0 ; i < 4 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        DumpByte( File );
        DumpByte( File );
        DumpByte( File );

        DumpFloat( File );
        DumpFloat( File );

        for ( int i = 0 ; i < 24 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        for ( int i = 0 ; i < 8 ; i++ ) {
            if ( DumpByte( File ) != 0xCD ) {
                Out() << "not CD";
            }
        }

        for ( int i = 0 ; i < 4 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        // Looks like vector
        DumpDouble( File );
        DumpDouble( File );
        DumpDouble( File );

        SetDumpLog( true );

        int32 FaceCount;
        File->ReadSwapInt32( FaceCount );

        if (FaceCount<4||FaceCount>100 ) {
            Out() << "WARNING: FILE READ ERROR.. SOMETHING GO WRONG!";
            Sectors.Resize(SectorIndex);
            assert(0);
            break;
        }

        int FirstFace = Sector.Faces.Length();

        Sector.Faces.Resize( FirstFace + FaceCount );

        for ( int FaceIndex = FirstFace ; FaceIndex < Sector.Faces.Length() ; FaceIndex++ ) {
            Sector.Faces[FaceIndex] = CreateFace();
            Sector.Faces[FaceIndex]->SectorIndex = SectorIndex;
            LoadFace( Sector.Faces[FaceIndex], SectorIndex == Sectors.Length() - 1, FaceIndex == Sector.Faces.Length() - 1 );
        }
    }

#if 0
    typedef enum FileEndChanks {
        FEC_Unk1 = 0x00003A99, //15001
        FEC_Unk2 = 0x00003A9A, //15002
    } FileEndChanks;


    int elemCount = DumpInt( File );
    for (int i = 0; i < elemCount; i++)
    {
        int Type = DumpInt( File );
        switch ( Type ) {
            case FEC_Unk1: { // pos += __addStructureAt(pos, "FileEndUnk1", "") ;
                DumpUnknownBytes( File, 11 );   // blob Unk1[ 11 ];

                // Vertex Unk2;
                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );

                DumpInt( File );

                break;
            }
            case FEC_Unk2: { // pos += __addStructureAt(pos, "FileEndUnk2", "") ;
                DumpUnknownBytes( File, 47 );   // blob Unk1[ 47 ];

                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );

                int NumIndices = DumpInt( File );
                for ( int j = 0 ; j < NumIndices ; j++ ) {
                    DumpInt( File );
                }
                
                break;
            }
            default:
                break;
        }
    }

    //pos += __addStructureAt(pos, "Vertex", "");
    Bounds.mins[0] = DumpDouble( File ) * 0.001f;
    Bounds.mins[1] = -DumpDouble( File ) * 0.001f;
    Bounds.mins[2] = -DumpDouble( File ) * 0.001f;

    //pos += __addStructureAt(pos, "Vertex", "");
    Bounds.maxs[0] = DumpDouble( File ) * 0.001f;
    Bounds.maxs[1] = -DumpDouble( File ) * 0.001f;
    Bounds.maxs[2] = -DumpDouble( File ) * 0.001f;    

    //pos += secCount * 4;
    File->Seek( SectorsCount * 4, FFileAbstract::SeekCur );

    //__addStructureAt(pos, "StringArray", "");
    TArrayList< FString > Strings;
    Strings.Resize( DumpInt( File ) );
    for ( int i = 0 ; i < Strings.Length() ; i++ ) {
        Strings[i] = DumpString( File );
    }
#endif
    FFiles::CloseFile( File );

    WorldGeometryPostProcess();
}

FBladeWorld::FFace * FBladeWorld::CreateFace() {
    Faces.Append( new FFace );
    return Faces.Last();
}
FBladeWorld::FPortal * FBladeWorld::CreatePortal() {
    Portals.Append( new FPortal );
    return Portals.Last();
}

void FBladeWorld::LoadFace( FFace * _Face, bool _LastSector, bool _LastFace ) {
    File->ReadSwapInt32( _Face->Type );

    switch ( _Face->Type ) {
        case FT_SimpleFace:
            Out() << "SimpleFace";
            LoadSimpleFace( _Face );
            break;
        case FT_Portal:
            Out() << "Portal";
            LoadPortalFace( _Face );
            break;
        case FT_Face:
            Out() << "Face";
            LoadFaceWithHole( _Face );
            break;
        case FT_Unknown:
            Out() << "Unknown";
            LoadUnknownFace( _Face, _LastSector, _LastFace );
            break;
        case FT_Skydome:
            Out() << "Skydome";
            LoadSkydomeFace( _Face );
            break;
        default:
            assert(0);
            break;
    }
}

void FBladeWorld::ReadIndices( TMutableArray< unsigned int > & _Indices ) {
    int32 NumIndices;
    File->ReadSwapInt32( NumIndices );
    _Indices.Resize( NumIndices );
    for ( int k = 0 ; k < NumIndices ; k++ ) {
        File->ReadSwapUInt32( _Indices[k] );
    }
}

void FBladeWorld::ReadWinding( TPolygon< double > & _Winding ) {
    int32 NumVertices;
    int Index;
    File->ReadSwapInt32( NumVertices );
    if ( NumVertices == 0 || NumVertices > 32 ) {
        Out() << "WARNING" << NumVertices << "SOMETHING GO WRONG!";
        assert( 0 );
    }
    _Winding.Resize( NumVertices );
    for ( int k = 0 ; k < NumVertices ; k++ ) {
        File->ReadSwapInt32( Index );
        _Winding[ NumVertices - k - 1 ] = Vertices[ Index ];
    }
}

void FBladeWorld::LoadSimpleFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.normal );
    File->ReadSwapDouble( _Face->Plane.d );

    // FIXME: What is it?
    File->ReadSwapUInt64( _Face->UnknownSignature );
    if ( _Face->UnknownSignature != 3 ) {
        Out() << "Face signature" << _Face->UnknownSignature;
    }

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );

    // Winding
    ReadIndices( _Face->Indices );
}

void FBladeWorld::LoadPortalFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.normal );
    File->ReadSwapDouble( _Face->Plane.d );

    // Winding
    ReadIndices( _Face->Indices );

    FPortal * Portal = CreatePortal();

    Portal->Face = _Face;
    File->ReadSwapInt32( Portal->ToSector );
    Portal->Winding.Resize( _Face->Indices.Length() );
    for ( int k = 0 ; k < _Face->Indices.Length() ; k++ ) {
        Portal->Winding[ _Face->Indices.Length() - k - 1 ] = Vertices[ _Face->Indices[k] ];
    }

    Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

    // FIXME: What is it?
    SetDumpLog( false );
    DumpUnknownBytes( File, 8 );
    SetDumpLog( true );

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );
}

void FBladeWorld::LoadFaceWithHole( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.normal );
    File->ReadSwapDouble( _Face->Plane.d );

    // FIXME: What is it?
    File->ReadSwapUInt64( _Face->UnknownSignature );
    if ( _Face->UnknownSignature != 3 ) {
        Out() << "Face signature" << _Face->UnknownSignature;
    }

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );

    // Winding
    TPolygon< double > Winding;
    ReadWinding( Winding );

    // Winding hole
    TPolygon< double > Hole;
    ReadWinding( Hole );

    FClipper Clipper;

    FDPlane Plane = _Face->Plane;//Winding.CalcPlane();

    Clipper.SetNormal( Plane.normal );
    Clipper.AddContour3D( Winding.ToPtr(), Winding.Length(), true );
    Clipper.AddContour3D( Hole.ToPtr(), Hole.Length(), false );

    TArrayList< FClipperPolygon > ResultPolygons;
    Clipper.Execute( FClipper::CLIP_DIFF, ResultPolygons );

    //PrintPolygons( ResultPolygons );

    typedef TTriangulator< FDVec2, FDVec2 > FTriangulator;
    FTriangulator Triangulator;
    TMutableArray< FTriangulator::FPolygon * > Polygons;
    TArrayList< FDVec2 > ResultVertices;
    for ( int i = 0 ; i < ResultPolygons.Length() ; i++ ) {
        FTriangulator::FPolygon * Polygon = new FTriangulator::FPolygon;

        Polygon->OuterContour = ResultPolygons[i].Outer.ToPtr();
        Polygon->OuterContourEnd = ResultPolygons[i].Outer.ToPtr() + ResultPolygons[i].Outer.Length();

        Polygon->HoleContours.Resize( ResultPolygons[i].Holes.Length() );
        Polygon->HoleContoursEnd.Resize( ResultPolygons[i].Holes.Length() );
        for ( int j = 0 ; j < ResultPolygons[i].Holes.Length() ; j++ ) {
            Polygon->HoleContours[j] = ResultPolygons[i].Holes[j].ToPtr();
            Polygon->HoleContoursEnd[j] = ResultPolygons[i].Holes[j].ToPtr() + ResultPolygons[i].Holes[j].Length();
        }

        Polygon->Normal.x = 0;
        Polygon->Normal.y = 0;
        Polygon->Normal.z = 1;

        Polygons.Append( Polygon );
    }
    Triangulator.TriangulatePolygons( Polygons, ResultVertices, _Face->Indices );

    // free polygons
    for ( int i = 0 ; i < Polygons.Length() ; i++ ) {
        FTriangulator::FPolygon * Polygon = Polygons[ i ];
        delete Polygon;
    }

    const FDMat3 & TransformMatrix = Clipper.GetTransform3D();

    _Face->Vertices.Resize( ResultVertices.Length() );
    for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
        _Face->Vertices[k] = TransformMatrix * FDVec3( ResultVertices[k], Plane.Dist() );
    }

    FPortal * Portal = CreatePortal();

    Portal->Face = _Face;
    File->ReadSwapInt32( Portal->ToSector );
    Portal->Winding = Hole;

    Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

    int32 Count;
    File->ReadSwapInt32( Count );
    Portal->Planes.Resize( Count );
    for ( int k = 0 ; k < Count ; k++ ) {
        File->ReadSwapVector( Portal->Planes[ k ].normal );
        File->ReadSwapDouble( Portal->Planes[ k ].d );
    }
}

void FBladeWorld::LoadUnknownFace( FFace * _Face, bool _LastSector, bool _LastFace ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.normal );
    File->ReadSwapDouble( _Face->Plane.d );

    // FIXME: What is it?
    File->ReadSwapUInt64( _Face->UnknownSignature );
    if ( _Face->UnknownSignature != 3 ) {
        Out() << "Face signature" << _Face->UnknownSignature;
    }

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );

    // Winding
    ReadIndices( _Face->Indices );

    TPolygon< double > Winding;

    Winding.Resize( _Face->Indices.Length() );
    for ( int k = 0 ; k < _Face->Indices.Length() ; k++ ) {
        Winding[ k ] = Vertices[ _Face->Indices[ k ] ];
    }
    Winding.Reverse();

    FDPlane Plane = _Face->Plane;

    int NumHoles;
    File->ReadSwapInt32( NumHoles );
    if ( NumHoles > 0 ) {
#if 0
        FClipper Clipper;
        TPolygon< double > Hole;

        Clipper.SetNormal( Plane.normal );
        Clipper.AddContour3D( Winding.ToPtr(), Winding.Length(), true );

        for ( int c = 0 ; c < NumHoles ; c++ ) {
            ReadWinding( Hole );

            Clipper.AddContour3D( Hole.ToPtr(), Hole.Length(), false );

            FPortal * Portal = CreatePortal();

            Portal->Face = _Face;
            File->ReadSwapInt32( Portal->ToSector );
            Portal->Winding = Hole;

            Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

            int32 Count;
            File->ReadSwapInt32( Count );
            Portal->Planes.Resize( Count );
            for ( int k = 0 ; k < Count ; k++ ) {
                File->ReadSwapVector( Portal->Planes[k].normal );
                File->ReadSwapDouble( Portal->Planes[k].d );
            }
        }

        TArrayList< FClipperPolygon > ResultPolygons;
        Clipper.Execute( FClipper::CLIP_DIFF, ResultPolygons );

        //PrintPolygons( ResultPolygons );

        typedef TTriangulator< FDVec2, FDVec2 > FTriangulator;
        FTriangulator Triangulator;
        TMutableArray< FTriangulator::FPolygon * > Polygons;
        TArrayList< FDVec2 > ResultVertices;
        TMutableArray< unsigned int > ResultIndices;
        for ( int i = 0 ; i < ResultPolygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = new FTriangulator::FPolygon;

            Polygon->OuterContour = ResultPolygons[i].Outer.ToPtr();
            Polygon->OuterContourEnd = ResultPolygons[i].Outer.ToPtr() + ResultPolygons[i].Outer.Length();

            Polygon->HoleContours.Resize( ResultPolygons[i].Holes.Length() );
            Polygon->HoleContoursEnd.Resize( ResultPolygons[i].Holes.Length() );
            for ( int j = 0 ; j < ResultPolygons[i].Holes.Length() ; j++ ) {
                Polygon->HoleContours[j] = ResultPolygons[i].Holes[j].ToPtr();
                Polygon->HoleContoursEnd[j] = ResultPolygons[i].Holes[j].ToPtr() + ResultPolygons[i].Holes[j].Length();
            }

            Polygon->Normal.x = 0;
            Polygon->Normal.y = 0;
            Polygon->Normal.z = 1;

            Polygons.Append( Polygon );
        }
        Triangulator.TriangulatePolygons( Polygons, ResultVertices, ResultIndices );

        // free polygons
        for ( int i = 0 ; i < Polygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = Polygons[ i ];
            delete Polygon;
        }

        const FDMat3 & TransformMatrix = Clipper.GetTransform3D();

        _Face->Vertices.Resize( ResultVertices.Length() );
        for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
            _Face->Vertices[k] = TransformMatrix * FDVec3( ResultVertices[k], Plane.Dist() );
        }
        _Face->Indices = ResultIndices;

#else
        FClipper HolesUnion;
        TPolygon< double > Hole;

        HolesUnion.SetNormal( Plane.normal );

        for ( int c = 0 ; c < NumHoles ; c++ ) {
            ReadWinding( Hole );

            HolesUnion.AddContour3D( Hole.ToPtr(), Hole.Length(), true );

            FPortal * Portal = CreatePortal();

            Portal->Face = _Face;
            File->ReadSwapInt32( Portal->ToSector );
            Portal->Winding = Hole;

            Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

            int32 Count;
            File->ReadSwapInt32( Count );
            Portal->Planes.Resize( Count );
            for ( int k = 0 ; k < Count ; k++ ) {
                File->ReadSwapVector( Portal->Planes[k].normal );
                File->ReadSwapDouble( Portal->Planes[k].d );
            }
        }

        // FIXME: Union of hulls may produce inner holes!
        TArrayList< FClipperContour > Holes;
        HolesUnion.Execute( FClipper::CLIP_UNION, Holes );

        FClipper Clipper;
        Clipper.SetNormal( Plane.normal );
        Clipper.AddContour3D( Winding.ToPtr(), Winding.Length(), true );

        for ( int i = 0 ; i < Holes.Length() ; i++ ) {
            Clipper.AddContour2D( Holes[i].ToPtr(), Holes[i].Length(), false, true );
        }

        TArrayList< FClipperPolygon > ResultPolygons;
        Clipper.Execute( FClipper::CLIP_DIFF, ResultPolygons );

        //PrintPolygons( ResultPolygons );

        typedef TTriangulator< FDVec2, FDVec2 > FTriangulator;
        FTriangulator Triangulator;
        TMutableArray< FTriangulator::FPolygon * > Polygons;
        TArrayList< FDVec2 > ResultVertices;
        TMutableArray< unsigned int > ResultIndices;
        for ( int i = 0 ; i < ResultPolygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = new FTriangulator::FPolygon;

            Polygon->OuterContour = ResultPolygons[i].Outer.ToPtr();
            Polygon->OuterContourEnd = ResultPolygons[i].Outer.ToPtr() + ResultPolygons[i].Outer.Length();

            Polygon->HoleContours.Resize( ResultPolygons[i].Holes.Length() );
            Polygon->HoleContoursEnd.Resize( ResultPolygons[i].Holes.Length() );
            for ( int j = 0 ; j < ResultPolygons[i].Holes.Length() ; j++ ) {
                Polygon->HoleContours[j] = ResultPolygons[i].Holes[j].ToPtr();
                Polygon->HoleContoursEnd[j] = ResultPolygons[i].Holes[j].ToPtr() + ResultPolygons[i].Holes[j].Length();
            }

            Polygon->Normal.x = 0;
            Polygon->Normal.y = 0;
            Polygon->Normal.z = 1;

            Polygons.Append( Polygon );
        }
        Triangulator.TriangulatePolygons( Polygons, ResultVertices, ResultIndices );

        // free polygons
        for ( int i = 0 ; i < Polygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = Polygons[ i ];
            delete Polygon;
        }

        const FDMat3 & TransformMatrix = Clipper.GetTransform3D();

        _Face->Vertices.Resize( ResultVertices.Length() );
        for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
            _Face->Vertices[k] = TransformMatrix * FDVec3( ResultVertices[k], Plane.Dist() );
        }
        _Face->Indices = ResultIndices;
#endif
    }

#if 0
    ReadFaceBSP( _Face );
#else
    //----------------------------------------------------------------------
    //
    // HACKY SECTION
    //
    if ( !_LastFace ) {
        // Find next face origin
        bool Done = false;
        while ( !Done ) {
            int32 Type;
            if ( File->ReadSwapInt32( Type ) < 4 ) {
                break;
            }
            switch ( Type ) {
            case FT_SimpleFace:
            case FT_Portal:
            case FT_Face:
            case FT_Unknown:
            case FT_Skydome:
                Done = true;
                File->Seek( -4, FFileAbstract::SeekCur );
                break;
            default:
                Out() << "Unknown byte:"<< FMath::ToHexString( byte(Type & 0xff) );
                File->Seek( -3, FFileAbstract::SeekCur );
                break;
            }
        }
    } else {

        if ( _LastSector ) {
            // Last sector and last face. Stop parsing
            return;
        }

        // Find next sector origin
        bool Done = false;
        while ( !Done ) {
            int32 StrLen;
            int Ofs = File->Tell();
            if ( File->ReadSwapInt32( StrLen ) < 4 ) {
                break;
            }
            bool bValidString = true;
            if ( StrLen >= 3 && StrLen < 32 ) {
                for ( int k = 0 ; k < StrLen ; k++ ) {
                    byte b = 0;
                    File->ReadByte( b );
                    if ( b >= 'a' && b <= 'z' || b >= 'A' && b <= 'Z' || b >= '0' && b <= '9' || b == '.' || b == '-' ) {
                        continue;
                    }
                    bValidString = false;
                    break;
                }
                if ( bValidString ) {
                    File->Seek( Ofs-8, FFileAbstract::SeekSet );
                    int64 i64;
                    File->ReadSwapInt64( i64 );
                    if ( i64 == 3 ) {
                        File->Seek( Ofs + 1, FFileAbstract::SeekSet );
                        continue;
                    }

                    //File->Seek( Ofs, FFileAbstract::SeekSet );
                    Done = true;
                } else {
                    File->Seek( Ofs + 1, FFileAbstract::SeekSet );
                }
            } else {
                File->Seek( Ofs + 1, FFileAbstract::SeekSet );
            }
        }
    }
    //----------------------------------------------------------------------
#endif
}

enum BSPNodeType {
    NT_Simple = 0x00001F41,    // 8001
    NT_Hard   = 0x00001F42,    // 8002
    NT_Leaf   = 0x00001F43,    // 8003
};

struct LeafWinding {
    uint32 UnknownIndex;
    TMutableArray< uint32 > Indices;
};

struct BSPNode_Leaf {
    BSPNodeType Type;
    TArrayList< LeafWinding > Windings;
};

struct BSPNode_Hard_end
{
    uint64 UnknownSignature;
    FString Name;
    float Unk4[16];
};

struct BSPNode_Hard_begin
{
    BSPNodeType sftype;
    BSPNode_Leaf One;
    BSPNode_Leaf Second;
    FDPlane Plane;
    BSPNode_Hard_end End;
};

struct BSPNode_Simple_begin
{
    BSPNodeType     sftype;
    BSPNode_Leaf    One;
    BSPNode_Leaf    Second;
    FDPlane         Plane;
};

void FBladeWorld::ReadBSPNode( BSPNode_Leaf & _Node, bool _Recursive ) {
    _Node.Type = ( BSPNodeType )DumpInt( File );
    
    int Count = DumpInt( File );

    _Node.Windings.Resize( Count );

    for ( int i = 0 ; i < Count ; i++ ) {
        LeafWinding & Winding = _Node.Windings[i];

        Winding.UnknownIndex = DumpInt( File );

        uint32 IndicesCount = DumpInt( File );
        Winding.Indices.Resize( IndicesCount );
        for ( int j = 0 ; j < IndicesCount ; j++ ) {
            Winding.Indices[j] = DumpInt( File );
        }
    }

    if ( _Recursive ) {
        BSPNode_Leaf Child0, Child1;
        ReadBSPNode( Child0, true );
        ReadBSPNode( Child1, true );
    }
}

void FBladeWorld::ReadFaceBSP_r( FFace * _Face ) {
#if 0
    uint32_t Type = DumpIntNotSeek( File );

    if ( Type == NT_Leaf ) {

        BSPNode_Leaf Leaf;
        ReadBSPNode( Leaf );

        ReadFaceBSP_r( _Face );


        if ( ( simpleStack & ( 1 << deep ) ) == 0 )//first time
        {
            simpleStack |= ( 1 << deep );
            pos += __addStructureAt( pos, "BSPNode_Leaf", "" );
        }

        while ( __getUDWordAt( pos ) == NT_Leaf ) {
            pos += __addStructureAt( pos, "BSPNode_Leaf", "" );
            while ( ( deep >= 0 ) && ( ( simpleStack & ( 1 << deep ) ) != 0 ) ) {
                simpleStack -= ( 1 << deep );
                pos += __addStructureAt( pos, "Plane", "" ) ;
                deep--;
            }
            simpleStack |= ( 1 << ( deep ) );
        }
    }
    deep++;


    while ( __getUDWordAt( pos ) == NT_Simple ) {
        pos += __addStructureAt( pos, "BSPNode_Simple_begin", "" ) ;

        if ( __getUDWordAt( pos ) == NT_Leaf ) {

            if ( ( simpleStack & ( 1 << deep ) ) == 0 )//first time
            {
                simpleStack |= ( 1 << deep );
                pos += __addStructureAt( pos, "BSPNode_Leaf", "" );
            }

            while ( __getUDWordAt( pos ) == NT_Leaf ) {
                pos += __addStructureAt( pos, "BSPNode_Leaf", "" );
                while ( ( deep >= 0 ) && ( ( simpleStack & ( 1 << deep ) ) != 0 ) ) {
                    simpleStack -= ( 1 << deep );
                    pos += __addStructureAt( pos, "Plane", "" ) ;
                    deep--;
                }
                simpleStack |= ( 1 << ( deep ) );
            }
        }
        deep++;
    }
#endif
}

void FBladeWorld::Read_BSPNode_Hard_begin() {
    BSPNode_Hard_begin Hard;
    Hard.sftype = (BSPNodeType)DumpInt( File );
    ReadBSPNode( Hard.One, false );
    ReadBSPNode( Hard.Second, false );
    File->ReadSwapVector( Hard.Plane.normal );
    File->ReadSwapDouble( Hard.Plane.d );
    File->ReadSwapUInt64( Hard.End.UnknownSignature );
    File->ReadString( Hard.End.Name );
    for ( int i = 0 ; i < 16 ; i++ ) {
        File->ReadSwapFloat( Hard.End.Unk4[ i ] );
    }
}

void FBladeWorld::Read_BSPNode_Hard_end() {
    BSPNode_Hard_end End;

    File->ReadSwapUInt64( End.UnknownSignature );
    File->ReadString( End.Name );
    for ( int i = 0 ; i < 16 ; i++ ) {
        File->ReadSwapFloat( End.Unk4[ i ] );
    }
}

void FBladeWorld::Read_BSPNode_Simple_begin() {
    BSPNode_Simple_begin Simple;
    Simple.sftype = (BSPNodeType)DumpInt( File );
    ReadBSPNode( Simple.One, false );
    ReadBSPNode( Simple.Second, false );
    File->ReadSwapVector( Simple.Plane.normal );
    File->ReadSwapDouble( Simple.Plane.d );
}

void FBladeWorld::Read_BSPNode_Leaf() {
    BSPNode_Leaf Node;
    ReadBSPNode( Node, false );
}

void FBladeWorld::Read_Plane() {
    FDPlane Plane;
    File->ReadSwapVector( Plane.normal );
    File->ReadSwapDouble( Plane.d );
}

void FBladeWorld::ReadFaceBSP( FFace * _Face ) {
#if 0
    int simpleStack = 0;
    int deep = 0;
    int first = 0;

    uint32_t Type = DumpInt( File );

    if ( Type == NT_Hard ) {
#if 0
        BSPNode_Hard_begin Hard;
        Hard.sftype = (BSPNodeType)Type;
        ReadBSPNode( Hard.One );
        ReadBSPNode( Hard.Second );
        File->ReadSwapVector( Hard.Plane.normal );
        File->ReadSwapDouble( Hard.Plane.d );
        File->ReadSwapUInt64( Hard.End.UnknownSignature );
        File->ReadString( Hard.End.Name );
        for ( int i = 0 ; i < 16 ; i++ ) {
            File->ReadSwapFloat( Hard.End.Unk4[ i ] );
        }

        ReadFaceBSP_r( _Face );


        //pos += __addStructureAt( pos, "BSPNode_Hard_end", "" ) ;
        File->ReadSwapUInt64( Hard.End.UnknownSignature );
        File->ReadString( Hard.End.Name );
        for ( int i = 0 ; i < 16 ; i++ ) {
            File->ReadSwapFloat( Hard.End.Unk4[ i ] );
        }
#endif
    } else if ( Type == NT_Simple ) {

        while ( Type == NT_Simple ) {

            BSPNode_Simple_begin Simple;
            Simple.sftype = (BSPNodeType)DumpInt( File );
            ReadBSPNode( Simple.One, false );
            ReadBSPNode( Simple.Second, false );
            File->ReadSwapVector( Simple.Plane.normal );
            File->ReadSwapDouble( Simple.Plane.d );

            DumpInt( File );
            DumpInt( File );
            DumpInt( File );
            DumpInt( File );


            //pos += __addStructureAt( pos, "BSPNode_Simple_begin", "" ) ;
            //if ( __getUDWordAt( pos ) == NT_Leaf ) {
            //    if ( ( simpleStack & ( 1 << deep ) ) == 0 )//first time
            //    {
            //        simpleStack |= ( 1 << deep );
            //        pos += __addStructureAt( pos, "BSPNode_Leaf", "" );
            //    }
            //    while ( __getUDWordAt( pos ) == NT_Leaf ) {
            //        pos += __addStructureAt( pos, "BSPNode_Leaf", "" );
            //        while ( ( deep >= 0 ) && ( ( simpleStack & ( 1 << deep ) ) != 0 ) ) {
            //            simpleStack -= ( 1 << deep );
            //            pos += __addStructureAt( pos, "Plane", "" ) ;
            //            deep--;
            //        }
            //        simpleStack |= ( 1 << ( deep ) );
            //    }
            //}
            //deep++;
            //Type = __getUDWordAt( pos ) == NT_Simple;
        }

    }
#endif

#define GetUint() DumpIntNotSeek(File)

    int simpleStack = 0;
    int deep = 0;
    int first = 0;
    //несмотря на вырвиглазный код, тут идет простой разбор BSPдерева
    //такой код, т.к. тут нет рекурсии
    int Type = GetUint();
    switch ( Type ) {
        case NT_Hard:
            Read_BSPNode_Hard_begin();
            while ( ( first == 0 ) || ( GetUint() == NT_Simple ) ) {
                if ( first == 0 )                     {
                    first = 1;
                }
                else
                    Read_BSPNode_Simple_begin();
                if ( GetUint() == NT_Leaf ) {
                    if ( ( simpleStack & ( 1 << deep ) ) == 0 )//first time
                    {
                        simpleStack |= ( 1 << deep );
                        Read_BSPNode_Leaf();
                    }
                    while ( GetUint() == NT_Leaf ) {
                        Read_BSPNode_Leaf();
                        while ( ( deep >= 0 ) && ( ( simpleStack & ( 1 << deep ) ) != 0 ) ) {
                            simpleStack -= ( 1 << deep );
                            Read_Plane();
                            deep--;
                        }
                        simpleStack |= ( 1 << ( deep ) );
                    }
                }
                deep++;
            }
            Read_BSPNode_Hard_end();
            break;
        case NT_Simple:
            while ( GetUint() == NT_Simple ) {
                Read_BSPNode_Simple_begin();
                if ( GetUint() == NT_Leaf ) {
                    if ( ( simpleStack & ( 1 << deep ) ) == 0 )//first time
                    {
                        simpleStack |= ( 1 << deep );
                        Read_BSPNode_Leaf();
                    }
                    while ( GetUint() == NT_Leaf ) {
                        Read_BSPNode_Leaf();
                        while ( ( deep >= 0 ) && ( ( simpleStack & ( 1 << deep ) ) != 0 ) ) {
                            simpleStack -= ( 1 << deep );
                            Read_Plane();
                            deep--;
                        }
                        simpleStack |= ( 1 << ( deep ) );
                    }
                }
                deep++;
            }
            break;
        default:
            break;
    }
}

void FBladeWorld::LoadSkydomeFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.normal );
    File->ReadSwapDouble( _Face->Plane.d );

    // Winding
    ReadIndices( _Face->Indices );
}

//#define USE_TEXCOORD_CORRECTION

static void RecalcTextureCoords( FBladeWorld::FFace * _Face, FMeshVertex * _Vertices, int _NumVertices, int _TexWidth, int _TexHeight ) {
#ifdef USE_TEXCOORD_CORRECTION
    FVec2 Mins( std::numeric_limits< float >::max() );
#endif

    double tx, ty;
    double sx = 1.0 / _TexWidth;
    double sy = 1.0 / _TexHeight;

    for ( int k = 0 ; k < _NumVertices ; k++ ) {
        FMeshVertex & Vert = _Vertices[ k ];

        tx = FMath::Dot( _Face->TexCoordAxis[ 0 ], FDVec3( Vert.Position ) ) + _Face->TexCoordOffset[ 0 ];
        ty = FMath::Dot( _Face->TexCoordAxis[ 1 ], FDVec3( Vert.Position ) ) + _Face->TexCoordOffset[ 1 ];
        tx *= sx;
        ty *= sy;

        Vert.TexCoord.s = tx;
        Vert.TexCoord.t = ty;

#ifdef USE_TEXCOORD_CORRECTION
        Mins.x = FMath::Min( Vert.TexCoord.x, Mins.x );
        Mins.y = FMath::Min( Vert.TexCoord.y, Mins.y );
#endif
    }

#ifdef USE_TEXCOORD_CORRECTION
    // Скорректировать текстурные координаты, чтобы они были ближе к нулю
    Mins.x = FMath::Floor( Mins.x );
    Mins.y = FMath::Floor( Mins.y );
    for ( int k = 0 ; k < _NumVertices ; k++ ) {
        FMeshVertex & Vert = _Vertices[ k ];
        Vert.TexCoord -= Mins;
    }
#endif
}

static void ConvertFacePlane( FDPlane & _Plane ) {
    _Plane.normal.y = -_Plane.normal.y;
    _Plane.normal.z = -_Plane.normal.z;
    _Plane.d *= 0.001f;
}

// Generate world mesh
void FBladeWorld::WorldGeometryPostProcess() {
    FMeshOffset MeshOffset;
    FMeshVertex Vertex;
    int FirstVertex;

    memset( &Vertex, 0, sizeof( Vertex ) );
    FirstVertex = 0;

    MeshOffset.BaseVertexLocation = 0;
    MeshOffset.StartIndexLocation = 0;
    MeshOffset.IndexCount = 0;

    for ( int SectorIndex = 0 ; SectorIndex < Sectors.Length() ; SectorIndex++ ) {
        FBladeWorld::FSector & Sector = Sectors[ SectorIndex ];

        Sector.Bounds.Clear();

        for ( int i = 0; i < Sector.Faces.Length() ; i++ ) {
            FBladeWorld::FFace * Face = Sector.Faces[ i ];

            ConvertFacePlane( Face->Plane );

            if ( Face->Type == FT_Portal ) {
                continue;
            }

            MeshOffset.StartIndexLocation = MeshIndices.Length();

            if ( Face->Vertices.Length() > 0 ) {
                for ( int v = 0 ; v < Face->Vertices.Length() ; v++ ) {
                    Vertex.Position.x = Face->Vertices[ v ].x * BLADE_COORD_SCALE;
                    Vertex.Position.y = Face->Vertices[ v ].y * BLADE_COORD_SCALE;
                    Vertex.Position.z = Face->Vertices[ v ].z * BLADE_COORD_SCALE;
                    Vertex.Normal = Face->Plane.normal;

                    MeshVertices.Append( Vertex );
                }

                for ( int j = 0 ; j < Face->Indices.Length() ; j++ ) {
                    MeshIndices.Append( FirstVertex + Face->Indices[ j ] );
                }

                MeshOffset.IndexCount = Face->Indices.Length();

                RecalcTextureCoords( Face, MeshVertices.ToPtr() + FirstVertex, Face->Vertices.Length(), 256, 256 );

                for ( int v = 0 ; v < Face->Vertices.Length() ; v++ ) {
                    FMeshVertex & Vert = MeshVertices.ToPtr()[ v + FirstVertex ];

                    Vert.Position.y = -Vert.Position.y;
                    Vert.Position.z = -Vert.Position.z;
                    Vert.Position *= 0.001f;

                    Sector.Bounds.AddPoint( Vert.Position );
                }

                FirstVertex += Face->Vertices.Length();

            } else {

                if ( Face->Indices.Length() < 3 ) {
                    continue;
                }

                for ( int j = 0 ; j < Face->Indices.Length() ; j++ ) {
                    int Index = Face->Indices[ j ];

                    Vertex.Position = Vertices[ Index ] * BLADE_COORD_SCALE;
                    Vertex.Normal = Face->Plane.normal;

                    MeshVertices.Append( Vertex );
                }

                for ( int j = 0 ; j < Face->Indices.Length() - 2 ; j++ ) {
                    MeshIndices.Append( FirstVertex + 0 );
                    MeshIndices.Append( FirstVertex + Face->Indices.Length() - j - 2 );
                    MeshIndices.Append( FirstVertex + Face->Indices.Length() - j - 1 );
                }

                MeshOffset.IndexCount = ( Face->Indices.Length() - 2 ) * 3;

                RecalcTextureCoords( Face, MeshVertices.ToPtr() + FirstVertex, Face->Indices.Length(), 256, 256 );

                for ( int v = 0 ; v < Face->Indices.Length() ; v++ ) {
                    FMeshVertex & Vert = MeshVertices.ToPtr()[ v + FirstVertex ];

                    Vert.Position.y = -Vert.Position.y;
                    Vert.Position.z = -Vert.Position.z;
                    Vert.Position *= 0.001f;

                    Sector.Bounds.AddPoint( Vert.Position );
                }

                FirstVertex += Face->Indices.Length();
            }

            MeshOffsets.Append( MeshOffset );
            MeshFaces.Append( Face );
        }
    }

    CalcTangentSpace( MeshVertices.ToPtr(), MeshVertices.Length(), MeshIndices.ToPtr(), MeshIndices.Length() );
}

void FBladeWorld::FreeWorld() {
    Atmospheres.Clear();
    Vertices.Clear();
    Sectors.Clear();
    MeshOffsets.Clear();
    MeshVertices.Clear();
    MeshIndices.Clear();
    MeshFaces.Clear();

    for ( int i = 0 ; i < Portals.Length() ; i++ ) {
        delete Portals[i];
    }
    Portals.Clear();

    for ( int i = 0 ; i < Faces.Length() ; i++ ) {
        delete Faces[i];
    }
    Faces.Clear();
}
