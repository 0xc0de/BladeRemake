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

FBladeWorld::FBSPNode * FBladeWorld::CreateBSPNode() {
    BSPNodes.Append( new FBSPNode );
    return BSPNodes.Last();
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
        case FT_FaceBSP:
            Out() << "FaceBSP";
            LoadFaceBSP( _Face, _LastSector, _LastFace );
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

static int VerticesOffset( const FDPlane & _Plane, const TMutableArray< FDVec3 > & _Vertices ) {
    int Front = 0;
    int Back = 0;

    for ( int i = 0 ; i < _Vertices.Length() ; i++ ) {
        EPlaneSide::Type Side = _Plane.SideOffset( _Vertices[i], 0.0 );

        if ( Side == EPlaneSide::Front ) {
            Front++;
        }
        if ( Side == EPlaneSide::Back ) {
            Back++;
        }
    }

    if ( Front > Back ) {
        return 0;
    }

    return 1;
}

void FBladeWorld::FilterWinding_r( FBladeWorld::FFace * _Face, FBSPNode * _Node, FBSPNode * _Leaf ) {
    if ( _Node->Type == NT_Leaf ) {
        return;
    }

    int Offset = VerticesOffset( _Node->Plane, _Leaf->Vertices );

    if ( Offset == 1 ) {        
        if ( _Node->Type == NT_TexInfo ) {
            _Leaf->TextureName = _Node->TextureName;
            _Leaf->TexCoordAxis[ 0 ] = _Node->TexCoordAxis[ 0 ];
            _Leaf->TexCoordAxis[ 1 ] = _Node->TexCoordAxis[ 1 ];
            _Leaf->TexCoordOffset[ 0 ] = _Node->TexCoordOffset[ 0 ];
            _Leaf->TexCoordOffset[ 1 ] = _Node->TexCoordOffset[ 1 ];
        }
        FilterWinding_r( _Face, _Node->Children[1], _Leaf );
    }    
}

void FBladeWorld::CreateWindings_r( FBladeWorld::FFace * _Face, const TArrayList< FClipperContour > & _Holes, TPolygon< double > * _Winding, FBSPNode * _Node ) {
    if ( _Node->Type == NT_Leaf ) {
        assert( _Winding != NULL );

        FClipper Clipper;
        Clipper.SetNormal( _Face->Plane.normal );
        Clipper.AddContour3D( _Winding->ToPtr(), _Winding->Length(), true );

        for ( int i = 0 ; i < _Holes.Length() ; i++ ) {
            Clipper.AddContour2D( _Holes[ i ].ToPtr(), _Holes[ i ].Length(), false, true );
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

            Polygon->OuterContour = ResultPolygons[ i ].Outer.ToPtr();
            Polygon->OuterContourEnd = ResultPolygons[ i ].Outer.ToPtr() + ResultPolygons[ i ].Outer.Length();

            Polygon->HoleContours.Resize( ResultPolygons[ i ].Holes.Length() );
            Polygon->HoleContoursEnd.Resize( ResultPolygons[ i ].Holes.Length() );
            for ( int j = 0 ; j < ResultPolygons[ i ].Holes.Length() ; j++ ) {
                Polygon->HoleContours[ j ] = ResultPolygons[ i ].Holes[ j ].ToPtr();
                Polygon->HoleContoursEnd[ j ] = ResultPolygons[ i ].Holes[ j ].ToPtr() + ResultPolygons[ i ].Holes[ j ].Length();
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

        _Node->Vertices.Resize( ResultVertices.Length() );
        for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
            _Node->Vertices[ k ] = TransformMatrix * FDVec3( ResultVertices[ k ], _Face->Plane.Dist() );
        }
        _Node->Indices = ResultIndices;

        Leafs.Append( _Node );

        return;
	}

    TPolygon< double > * Front = NULL;
    TPolygon< double > * Back = NULL;

    assert( _Winding != NULL );
    _Winding->Split( _Node->Plane, &Front, &Back, 0.0 );

    CreateWindings_r( _Face, _Holes, Front, _Node->Children[0] );
    CreateWindings_r( _Face, _Holes, Back, _Node->Children[1] );

    delete Front;
    delete Back;
}

void FBladeWorld::LoadFaceBSP( FFace * _Face, bool _LastSector, bool _LastFace ) {
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

    TArrayList< FClipperContour > Holes;

    int NumHoles;
    File->ReadSwapInt32( NumHoles );
    if ( NumHoles > 0 ) {
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
        HolesUnion.Execute( FClipper::CLIP_UNION, Holes );
    }

    _Face->Root = ReadBSPNode_r( _Face );

    // Create windings and fill Leafs
    CreateWindings_r( _Face, Holes, &Winding, _Face->Root );

    // Create subfaces
    for ( int i = 0 ; i < Leafs.Length() ; i++ ) {
        Leafs[i]->TextureName = _Face->TextureName;
        Leafs[i]->TexCoordAxis[0] = _Face->TexCoordAxis[0];
        Leafs[i]->TexCoordAxis[1] = _Face->TexCoordAxis[1];
        Leafs[i]->TexCoordOffset[0] = _Face->TexCoordOffset[0];
        Leafs[i]->TexCoordOffset[1] = _Face->TexCoordOffset[1];

        FilterWinding_r( _Face, _Face->Root, Leafs[i] );

        FBladeWorld::FFace * SubFace = CreateFace();
        SubFace->TextureName = Leafs[i]->TextureName;
        SubFace->TexCoordAxis[ 0 ] = Leafs[i]->TexCoordAxis[ 0 ];
        SubFace->TexCoordAxis[ 1 ] = Leafs[i]->TexCoordAxis[ 1 ];
        SubFace->TexCoordOffset[ 0 ] = Leafs[i]->TexCoordOffset[ 0 ];
        SubFace->TexCoordOffset[ 1 ] = Leafs[i]->TexCoordOffset[ 1 ];
        SubFace->SectorIndex = _Face->SectorIndex;
        SubFace->Plane = _Face->Plane;
        SubFace->Vertices = Leafs[i]->Vertices;
        SubFace->Indices = Leafs[i]->Indices;

        _Face->SubFaces.Append( SubFace );
    }
    Leafs.Clear();

    assert( _Face->SubFaces.Length() > 0 );
}

static void ConvertFacePlane( FDPlane & _Plane ) {
    _Plane.normal.y = -_Plane.normal.y;
    _Plane.normal.z = -_Plane.normal.z;
    _Plane.d *= 0.001f;
}

FBladeWorld::FBSPNode * FBladeWorld::ReadBSPNode_r( FFace * _Face ) {
    // Special thanks to ASP for node parsing

    FBSPNode * Node = CreateBSPNode();

    File->ReadSwapInt32( ( int32 & )Node->Type );

    if ( Node->Type == NT_Leaf ) {

        Node->Children[0] = NULL;
        Node->Children[1] = NULL;

        int Count = DumpInt( File );

        Node->Unknown.Resize( Count );

        for ( int i = 0; i < Count; i++ ) {
            FLeafIndices & Unknown = Node->Unknown[ i ];

            Unknown.UnknownIndex = DumpInt( File );

            uint32 IndicesCount = DumpInt( File );
            Unknown.Indices.Resize( IndicesCount );
            for ( int j = 0; j < IndicesCount; j++ ) {
                Unknown.Indices[ j ] = DumpInt( File );
            }
        }

        return Node;
	}    

    Node->Children[0] = ReadBSPNode_r( _Face );
    Node->Children[1] = ReadBSPNode_r( _Face );

    File->ReadSwapVector( Node->Plane.normal );
    File->ReadSwapDouble( Node->Plane.d );

    if ( Node->Type == NT_TexInfo ) {
        File->ReadSwapUInt64( Node->UnknownSignature );
        File->ReadString( Node->TextureName );

        File->ReadSwapVector( Node->TexCoordAxis[0] );
        File->ReadSwapVector( Node->TexCoordAxis[1] );
        File->ReadSwapFloat( Node->TexCoordOffset[0] );
        File->ReadSwapFloat( Node->TexCoordOffset[1] );

        Node->TexCoordOffset[ 0 ] = -Node->TexCoordOffset[ 0 ];
        Node->TexCoordOffset[ 1 ] = -Node->TexCoordOffset[ 1 ];

        // 8 zero bytes?
        for ( int k = 0 ; k < 8 ; k++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero!";
            }
        }
    }

    return Node;
}

void FBladeWorld::LoadSkydomeFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.normal );
    File->ReadSwapDouble( _Face->Plane.d );

    // Winding
    ReadIndices( _Face->Indices );
}

#define USE_TEXCOORD_CORRECTION

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

            if ( Face->Type == FT_FaceBSP ) {
                for ( int sf = 0; sf < Face->SubFaces.Length() ; sf++ ) {
                    FBladeWorld::FFace * SubFace = Face->SubFaces[ sf ];

                    assert( SubFace != Face );

                    ConvertFacePlane( SubFace->Plane );

                    MeshOffset.StartIndexLocation = MeshIndices.Length();

                    for ( int v = 0 ; v < SubFace->Vertices.Length() ; v++ ) {
                        Vertex.Position.x = SubFace->Vertices[ v ].x * BLADE_COORD_SCALE;
                        Vertex.Position.y = SubFace->Vertices[ v ].y * BLADE_COORD_SCALE;
                        Vertex.Position.z = SubFace->Vertices[ v ].z * BLADE_COORD_SCALE;
                        Vertex.Normal = Face->Plane.normal;

                        MeshVertices.Append( Vertex );
                    }

                    for ( int j = 0 ; j < SubFace->Indices.Length() ; j++ ) {
                        MeshIndices.Append( FirstVertex + SubFace->Indices[ j ] );
                    }

                    MeshOffset.IndexCount = SubFace->Indices.Length();

                    RecalcTextureCoords( SubFace, MeshVertices.ToPtr() + FirstVertex, SubFace->Vertices.Length(), 256, 256 );

                    for ( int v = 0 ; v < SubFace->Vertices.Length() ; v++ ) {
                        FMeshVertex & Vert = MeshVertices.ToPtr()[ v + FirstVertex ];

                        Vert.Position.y = -Vert.Position.y;
                        Vert.Position.z = -Vert.Position.z;
                        Vert.Position *= 0.001f;

                        Sector.Bounds.AddPoint( Vert.Position );
                    }

                    FirstVertex += SubFace->Vertices.Length();

                    MeshOffsets.Append( MeshOffset );
                    MeshFaces.Append( SubFace );
                }
            } else {
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

                    MeshOffsets.Append( MeshOffset );
                    MeshFaces.Append( Face );

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

    for ( int i = 0 ; i < BSPNodes.Length() ; i++ ) {
        delete BSPNodes[i];
    }
    BSPNodes.Clear();
}


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

#endif
