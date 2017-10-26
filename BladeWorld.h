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

#pragma once

#include "BladeMap.h"

#include <Framework/Geometry/Public/Polygon.h>
#include <Engine/Mesh/Public/StaticMeshResource.h>

// Blade .BW file loader

struct FBladeWorld {
    enum EFaceType {
        FT_SimpleFace = 0x00001B59,    // 7001 Face without holes/portals
        FT_Portal     = 0x00001B5A,    // 7002 Transparent wall (hole/portal)
        FT_Face       = 0x00001B5B,    // 7003 Face with one hole/protal
        FT_Unknown    = 0x00001B5C,    // 7004 Face with several holes/protals
        FT_Skydome    = 0x00001B5D     // 7005 Sky
    };

    struct FFace {
        int Type;
        FDPlane Plane;
        uint64 UnknownSignature;   // Только для фейсов с текстурой
        FString TextureName;        // Только для фейсов с текстурой
        FDVec3 TexCoordAxis[2];     // Только для фейсов с текстурой
        float TexCoordOffset[2];   // Только для фейсов с текстурой
        TMutableArray< unsigned int > Indices;

        // result mesh
        TMutableArray< FDVec3 > Vertices;

        int SectorIndex;

        void Duplicate( FFace * _Dst ) {
            _Dst->Type = Type;
            _Dst->Plane = Plane;
            _Dst->UnknownSignature = UnknownSignature;
            _Dst->TextureName = TextureName;
            _Dst->TexCoordAxis[0] = TexCoordAxis[0];
            _Dst->TexCoordAxis[1] = TexCoordAxis[1];
            _Dst->TexCoordOffset[0] = TexCoordOffset[0];
            _Dst->TexCoordOffset[1] = TexCoordOffset[1];
            _Dst->Indices = Indices;
            _Dst->Vertices = Vertices;
            _Dst->SectorIndex = SectorIndex;
        }
    };

    struct FPortal {
        FFace * Face;
        int32 ToSector;
        TPolygon< double > Winding;

        // Some planes. What they mean?
        TMutableArray< FDPlane > Planes;

        bool Marked;
    };

    struct FSector {
        // TODO: We can use this for color grading or light manipulation within the sector
        byte AmbientColor[ 3 ];
        float AmbientIntensity;

        TArrayList< FFace * > Faces;
        TArrayList< FPortal * > Portals;

        FAxisAlignedBox Bounds;
    };

    TArrayList< FBladeMap::FAtmosphereEntry > Atmospheres;
    TArrayList< FDVec3 > Vertices;
    TArrayList< FSector > Sectors;
    TArrayList< FMeshOffset > MeshOffsets;
    TArrayList< FMeshVertex > MeshVertices;
    TArrayList< unsigned int > MeshIndices;
    TArrayList< FFace * > MeshFaces;

    TMutableArray< FPortal * > Portals;
    TMutableArray< FFace * > Faces;

    FAxisAlignedBox Bounds;

    ~FBladeWorld();

    void LoadWorld( const char * _FileName );
    void FreeWorld();

private:
    FFace * CreateFace();
    FPortal * CreatePortal();

    void LoadFace( FFace * _Face, bool _LastSector, bool _LastFace );
    void LoadSimpleFace( FFace * _Face );
    void LoadPortalFace( FFace * _Face );
    void LoadFaceWithHole( FFace * _Face );
    void LoadUnknownFace( FFace * _Face, bool _LastSector, bool _LastFace );
    void LoadSkydomeFace( FFace * _Face );
    void ReadIndices( TMutableArray< unsigned int > & _Indices );
    void ReadWinding( TPolygon< double > & _Winding );
    void ReadFaceBSP_r( FFace * _Face );
    void ReadFaceBSP( FFace * _Face );
    void ReadBSPNode( struct BSPNode_Leaf & _Node, bool _Recursive );
    void Read_BSPNode_Hard_begin();
    void Read_BSPNode_Hard_end();
    void Read_BSPNode_Simple_begin();
    void Read_BSPNode_Leaf();
    void Read_Plane();
    void WorldGeometryPostProcess();

    FFileAbstract * File;
};

extern FBladeWorld World;
