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

#include "BOD.h"
#include "BladeWorld.h"
#include "BladeLevel.h"
#include "BladeTextures.h"
#include "BladeCSV.h"
#include "BladeSF.h"
#include "BladeCAM.h"
#include "World.h"
#include "ImGuiAPI.h"

#include <Framework/Platform/Public/PlatformWindow.h>
#include <Framework/Platform/Public/Thread.h>
#include <Framework/Platform/Public/Atomic.h>
#include <Framework/IO/Public/Logger.h>
#include <Framework/IO/Public/Document.h>
#include <Framework/IO/Public/FileUrl.h>
#include <Framework/IO/Public/FileMemory.h>
#include <Framework/Memory/Public/Alloc.h>
#include <Framework/Math/Public/ColorSpace.h>
#include <Framework/Cmd/Public/CmdManager.h>
#include <Framework/Cmd/Public/VarManager.h>

#include <Engine/EntryDecl.h>
#include <Engine/Window.h>

#include <Engine/Mesh/Public/StaticMeshComponent.h>
#include <Engine/Mesh/Public/SkinnedMeshComponent.h>
#include <Engine/Mesh/Public/SkinnedAnimComponent.h>
#include <Engine/Mesh/Public/StaticMeshResource.h>
#include <Engine/Mesh/Public/SkinnedAnimResource.h>
#include <Engine/Mesh/Public/DynamicMesh.h>

#include <Engine/Scene/Public/PrimitiveBatchComponent.h>
#include <Engine/Scene/Public/SpatialTreeComponent.h>
#include <Engine/Scene/Public/CameraComponent.h>
#include <Engine/Scene/Public/PlaneGridComponent.h>
#include <Engine/Scene/Public/SkyboxComponent.h>
#include <Engine/Scene/Public/LightComponent.h>
#include <Engine/Scene/Public/LogicComponent.h>

#include <Engine/AI/Public/ChunkedMeshComponent.h>

#include <Engine/Audio/Public/AudioResource.h>
#include <Engine/Audio/Public/AudioComponent.h>
#include <Engine/Audio/Public/AudioSourceComponent.h>
#include <Engine/Audio/Public/AudioListenerComponent.h>
#include <Engine/Audio/Public/AudioSystem.h>

#include <Engine/Physics/Public/PhysicsComponent.h>
#include <Engine/Physics/Public/RigidBodyComponent.h>
#include <Engine/Physics/Public/CollisionShapeComponent.h>

#include <Engine/Renderer/Public/Renderer.h>
#include <Engine/Renderer/Public/SpatialTreeProxy.h>

#include <Engine/Resource/Public/ResourceManager.h>

#include <Engine/Material/Public/TextureResource.h>
#include <Engine/Material/Public/MaterialResource.h>

//#define UNLIT
//#define DEBUG_BLADE_MP_SECTORS
//#define DEBUG_BLADE_FACE_PORTAL
//#define DEBUG_BLADE_GHOST_SECTORS
//#define DEBUG_BLADE_CAMERA_RECORDS
//#define DEBUG_CHARACTER_SELECTION
//#define DEBUG_WORLD_PICKING
//#define DEBUG_PORTALS
//#define DEBUG_MONITOR_GAMMA

// Config variables
static FCVarInt     demo_width( "demo_width", "1024" );
static FCVarInt     demo_height( "demo_height", "768" );
static FCVarBool    demo_fullscreen( "demo_fullscreen", "0" );
static FCVarBool    demo_ssao( "demo_ssao", "1" );
static FCVarBool    demo_colorgrading( "demo_colorgrading", "0" );
static FVariable    demo_gamepath( "demo_gamepath", "E:\\Games\\Blade Of Darkness" );
static FVariable    demo_gamelevel( "demo_gamelevel", "Maps/Mine_M5/mine.lvl" );
static FVariable    demo_music( "demo_music", "Sounds/MAPA2.mp3" );

// Common objects
static FWindow *                Window;             // Primary game window
static TSharedPtr< FScene >     Scene;              // Level scene
static FSceneNode *             CameraNode;         // View camera
static FCameraComponent *       Camera;             // View camera
static FTextureResource *       RenderTexture;      // Render target texture
static FRenderTarget *          RenderTarget;       // Render target owner
static FChunkedMeshComponent *  ChunkedMesh;        // Optimized world mesh storage for fast world-ray intersection

// For camera record debugging
static FCameraRecord amazona_barbaro[2];        // amazona   -> barbaro,   barbaro   -> amazona
static FCameraRecord barbaro_caballero[ 2 ];    // barbaro   -> caballero, caballero -> barbaro
static FCameraRecord caballero_enano[ 2 ];      // caballero -> enano,     enano     -> caballero
static FCameraRecord enano_amazona[ 2 ];        // enano     -> amazona,   amazona   -> enano
enum { amazona, barbario, caballero, enano };

static const char * MakePath( const char * _Path ) {
    static FString Tmp;
    Tmp = FString( demo_gamepath.GetString() ) + "/" + _Path;
    return Tmp.Str();
}

static void LoadConfigFile() {
    FFileAbstract * File = FFiles::OpenFileFromUrl( "blade.cfg", FFileAbstract::M_Read );
    if ( File ) {
        long Length = File->Length();
        char * Text = new char[Length+1];
        File->Read( Text, Length );
        Text[Length] = '\0';
        GCmdManager->BufferOp( ECmdBufferOperation::ExecuteText, Text );
        delete [] Text;
        FFiles::CloseFile( File );
    }
}

static void CreateAudioForSector( const FBladeSF::FGhostSector & _Sector ) {
    FVec3 Center(0);

    // Compute sector center
    int VerticesCount = _Sector.Vertices.Length();
    if ( VerticesCount > 0 ) {
        for ( int j = 0 ; j < VerticesCount ; j++ ) {
            const FVec2 & v = _Sector.Vertices[ j ];
            Center.x += v.x;
            Center.z += v.y;
        }
        Center.x /= VerticesCount;
        Center.z /= VerticesCount;
        Center.y = ( _Sector.FloorHeight + _Sector.RoofHeight ) * 0.5f;
    }

#if 0
    // Create trigger based on sector convex hull
    TMutableArray<FVec3> VertexSoup;
    for ( int j = 0 ; j < VerticesCount ; j++ ) {
        const FVec2 & v = Sector.Vertices[ j ];
        VertexSoup.Append( FVec3( v.x, Sector.FloorHeight, v.y ) );
    }
    for ( int j = 0 ; j < VerticesCount ; j++ ) {
        const FVec2 & v = Sector.Vertices[ j ];
        VertexSoup.Append( FVec3( v.x, Sector.RoofHeight, v.y ) );
    }
    FSceneNode * TriggerNode = Scene->CreateChild( "TriggerNode" );
    FRigidBodyComponent * RigidBody = TriggerNode->CreateComponent< FRigidBodyComponent >();
    RigidBody->SetTrigger( true );
    FCollisionShapeComponent * CollisionShape = TriggerNode->CreateComponent< FCollisionShapeComponent >();
    CollisionShape->SetConvexHull( VertexSoup.ToPtr(), VertexSoup.Length() );
#endif

    // Load audio
    FAudioResource * Resource = GResourceManager->GetResource< FAudioResource >( _Sector.Sound.Str() );
    FAudioResource::FLoadParameters LoadParameters;
    LoadParameters.StreamType = SST_NonStreamed;
    Resource->SetLoadParameters( LoadParameters );
    Resource->Load();

    // Create audio object
    FSceneNode * AudioNode = Scene->CreateChild( "SectorAudio" );

    // Locate audio node at center of ghost sector
    AudioNode->SetPosition( Center );

    // Create audio player
    FAudioSourceComponent * Audio = AudioNode->CreateComponent< FAudioSourceComponent >();
    Audio->SetAudio( Resource );
    Audio->SetTag( SND_SOUND );
    Audio->SetLooping( true );
    Audio->SetRelativeToListener( false );
    //Audio->SetGain( 1 );
    Audio->SetGain( _Sector.Volume );
    Audio->SetRolloffFactor( 1 );
    Audio->SetReferenceDistance( _Sector.MinDist * 0.001f );
    Audio->Play();

    // TODO: adapt ghost sector audio parameters to Angie Engine AudioSource parameters

    // distance = clamp(distance,MaxDistance,ReferenceDistance);
    // gain = ReferenceDistance / (ReferenceDistance +  RolloffFactor * (distance Ц ReferenceDistance) )
    //Audio->SetMaxDistance( _Sector.MinDist * 0.001f );
    //Audio->SetReferenceDistance( _Sector.MaxDist * 0.001f );
    //Audio->SetReferenceDistance( _Sector.MinDist * 0.001f );

    Out() << "AUDIO GROUP:" << _Sector.Group << _Sector.Sound;
}

static void LoadGhostSectors( const char * _FileName ) {
    FBladeSF SF;

    SF.LoadSF( _FileName );

    for ( int i = 0 ; i < SF.GhostSectors.Length() ; i++ ) {
        CreateAudioForSector( SF.GhostSectors[i] );
    }
}

static void LoadMusic() {
    FAudioResource::FLoadParameters LoadParameters;

    // Play and stream from file
    LoadParameters.StreamType = SST_FileStreamed;

    // Play and stream from memory
    //LoadParameters.StreamType = SST_MemoryStreamed;

    // Play from memory without streaming
    //LoadParameters.StreamType = SST_NonStreamed;

    // Load music resource
    FAudioResource * Snd = GResourceManager->GetResource< FAudioResource >( MakePath( demo_music.GetString() ) );
    Snd->SetLoadParameters( LoadParameters );
    Snd->Load();

    // Create music object
    FSceneNode * N = Scene->CreateChild( "Music" );

    // Create music player
    FAudioSourceComponent * Source = N->CreateComponent< FAudioSourceComponent >();
    Source->SetAudio( Snd );
    Source->SetLooping( true );
    Source->SetTag( SND_MUSIC );
    Source->SetRelativeToListener( true );
    Source->SetGain( 0.2f );
    Source->Play();
}

static void CreateAreasAndPortals() {
    TMutableArray< FSpatialAreaComponent * > Areas;

    Areas.Resize( World.Sectors.Length() );
    for ( int i = 0 ; i < Areas.Length() ; i++ ) {
        Areas[i] = Scene->CreateComponent< FSpatialAreaComponent >();
        Areas[i]->SetPosition( World.Sectors[i].Bounds.Center() );
        Areas[i]->SetBox( World.Sectors[i].Bounds.Size() + FVec3(0.01f) );
    }

    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        FBladeWorld::FSector & Sector = World.Sectors[i];
        for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
            Sector.Portals[j]->Marked = false;
        }
    }

    TPolygon< float > Winding;
    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        FBladeWorld::FSector & Sector = World.Sectors[i];

        for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
            FBladeWorld::FPortal * Portal = Sector.Portals[j];

            if ( Portal->Marked ) {
                continue;
            }

            FBladeWorld::FSector & Next = World.Sectors[Portal->ToSector];
            for ( int k = 0 ; k < Next.Portals.Length() ; k++ ) {
                FBladeWorld::FPortal * NextPortal = Next.Portals[k];
                if ( NextPortal->ToSector != i ) {
                    continue;
                }
                NextPortal->Marked = true;
            }

            Winding.Resize( Portal->Winding.Length() );
            for ( int k = 0 ; k < Winding.Length() ; k++ ) {
                Winding[k].x =  Portal->Winding[k].x*0.001f;
                Winding[k].y = -Portal->Winding[k].y*0.001f;
                Winding[k].z = -Portal->Winding[k].z*0.001f;
            }

            FSpatialPortalComponent * SpatialPortal = Scene->CreateComponent< FSpatialPortalComponent >();
            SpatialPortal->SetAreas( Areas[i], Areas[Portal->ToSector] );
            SpatialPortal->SetWinding( Winding.ToPtr(), Winding.Length() );
        }
    }
}

static void CreateCamera() {
    // Create camera object
    CameraNode = Scene->CreateChild( "Camera" );
    CameraNode->SetPosition( 0, 2, 5 );

    // Attach audio listener to camera
    CameraNode->CreateComponent< FAudioListenerComponent >();

    // Create camera
    Camera = CameraNode->CreateComponent< FCameraComponent >();
    Camera->SetFovX( 100 );
    //Camera->SetFovX( 80 );

    // Camera collision test
    //FRigidBodyComponent * RigidBody = CameraNode->CreateComponent< FRigidBodyComponent >();
    //RigidBody->SetKinematic( true );
    //RigidBody->SetMass( 1 );
    //FCollisionShapeComponent * CollisionShape = CameraNode->CreateComponent< FCollisionShapeComponent >();
    //CollisionShape->SetSphere( 1.0f );
}

static void CreateSunLight() {
    // Create light object
    FSceneNode * Node = Scene->CreateChild( "SunLight" );
    Node->SetPosition( 0, 5, 0 );

    // Attach directional light
    FLightComponent * Light = Node->CreateComponent< FLightComponent >();
    Light->SetType( FLightComponent::T_Direction );
    Light->SetColor( 1, 1, 1 );
    Light->SetDirection( FVec3( -0.2f, -1, 0 ) );
    Light->SetLumens( 1000 );
    Light->SetTemperature( 10000 );
}

static void CreateWorldGeometry() {
    FAxisAlignedBox Bounds;

    // Upload world mesh
    FStaticMeshResource * WorldMesh = GResourceManager->CreateUnnamedResource< FStaticMeshResource >();
    WorldMesh->SetVertexData( World.MeshVertices.ToPtr(), World.MeshVertices.Length(), World.MeshIndices.ToPtr(), World.MeshIndices.Length() );
    WorldMesh->SetMeshOffsets( World.MeshOffsets.ToPtr(), World.MeshOffsets.Length() );

    // Create materials
#ifdef UNLIT
    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/UnlitMaterial.json" );
#else
    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
#endif
    Material->Load();

    FMaterialResource * SkyboxMaterial = GResourceManager->GetResource< FMaterialResource >( "Blade/Skybox.json" );
    SkyboxMaterial->Load();

    FMaterialInstance * SkyboxMaterialInstance = SkyboxMaterial->CreateInstance();
    SkyboxMaterialInstance->Set( SkyboxMaterialInstance->AddressOf( "SmpCubemap" ), SkyboxTexture );

    // Create default texture
    FTextureResource * DefaultTexture = GResourceManager->GetResource< FTextureResource >( "Blade/mipmapchecker.png" );
    FTextureResource::FLoadParameters LoadParameters;
    LoadParameters.BuildMipmaps = true;
    LoadParameters.SRGB = true;
    DefaultTexture->SetLoadParameters( LoadParameters );
    DefaultTexture->Load();

    // Create world object
    FSceneNode * WorldNode = Scene->CreateChild( "World" );
    for ( int i = 0 ; i < World.MeshOffsets.Length() ; i++ ) {
        FMeshOffset & Ofs = World.MeshOffsets[i];
        FStaticMeshComponent * WorldRenderable = WorldNode->CreateComponent< FStaticMeshComponent >();
        WorldRenderable->SetMesh( WorldMesh );
        WorldRenderable->SetDrawRange( Ofs.IndexCount, Ofs.StartIndexLocation, Ofs.BaseVertexLocation );
        Bounds.Clear();
        for ( int k = 0 ; k < Ofs.IndexCount ; k++ ) {
            Bounds.AddPoint( World.MeshVertices[ Ofs.BaseVertexLocation + World.MeshIndices[ Ofs.StartIndexLocation + k ] ].Position );
        }
        WorldRenderable->SetBounds( Bounds );
        WorldRenderable->SetUseCustomBounds( true );

        FBladeWorld::FFace * Face = World.MeshFaces[i];
        if ( Face->Type == FBladeWorld::FT_Skydome || Face->TextureName.Length() == 0 ) {
            WorldRenderable->SetMaterialInstance( SkyboxMaterialInstance );
            WorldRenderable->EnableShadowCast( false );
        } else {
            FTextureResource * Texture = GResourceManager->GetResource< FTextureResource >( Face->TextureName.Str() );
            if ( !Texture->Load() ) {
                Texture = DefaultTexture;
            }

            FMaterialInstance * MaterialInstance = Material->CreateInstance();
            MaterialInstance->Set( MaterialInstance->AddressOf( "SmpBaseColor" ), Texture );

            FVec4 AmbientColor;
            const float ColorNormalizer = 1.0f / 255.0f;
            AmbientColor.x = World.Sectors[ Face->SectorIndex ].AmbientColor[0] * ColorNormalizer;
            AmbientColor.y = World.Sectors[ Face->SectorIndex ].AmbientColor[1] * ColorNormalizer;
            AmbientColor.z = World.Sectors[ Face->SectorIndex ].AmbientColor[2] * ColorNormalizer;
            AmbientColor.w = World.Sectors[ Face->SectorIndex ].AmbientIntensity;// * 5;

            #ifndef UNLIT
            AmbientColor.w *= 5;
            #endif

            MaterialInstance->Set( MaterialInstance->AddressOf( "AmbientColor" ), AmbientColor );
            WorldRenderable->SetMaterialInstance( MaterialInstance );
        }
    }

    // Create chunked mesh for world picking
#ifdef DEBUG_WORLD_PICKING
    ChunkedMesh = WorldNode->CreateComponent< FChunkedMeshComponent >();
    ChunkedMesh->Build();
#endif
}

static void CreateDebugMesh() {
    FSceneNode * Node = Scene->CreateChild( "DebugMesh" );
    FPrimitiveBatchComponent * Prim = Node->CreateComponent< FPrimitiveBatchComponent >();

    Prim->SetColor( 1, 0, 0, 1 );
    Prim->DrawAABB( World.Bounds );

#ifdef DEBUG_BLADE_MP_SECTORS
    FBladeMap Map;

    LoadMP( Map );

    Prim->SetPrimitive( P_LineLoop );

    for ( int SectorIndex = 0 ; SectorIndex < Map.Sectors.Length() ; SectorIndex++ ) {
        BladeMP::SectorEntry & Sector = Map.Sectors[ SectorIndex ];

        for ( int i = 0 ; i < Sector.Vertices.Length() ; i++ ) {
            BladeMP::SectorEntry::Vertex & Vertex = Sector.Vertices[i];

            Prim->EmitPoint( Vertex.X, Sector.FloorHeight, Vertex.Y );
        }

        Prim->Flush();

        for ( int i = 0 ; i < Sector.Vertices.Length() ; i++ ) {
            BladeMP::SectorEntry::Vertex & Vertex = Sector.Vertices[i];

            Prim->EmitPoint( Vertex.X, Sector.CeilHeight, Vertex.Y );
        }

        Prim->Flush();
    }

    Prim->SetPrimitive( P_Points );
    Prim->SetColor( 1,0,0,1 );
    for ( int i = 0 ; i < World.Vertices.Length() ; i++ ) {
        Prim->DrawCircle( FVec3( World.Vertices[i].X*0.001f,-World.Vertices[i].Y*0.001f,-World.Vertices[i].Z*0.001f ), 0.1f );
    }
    Prim->Flush();
#endif

#ifdef DEBUG_BLADE_FACE_PORTAL
    Prim->SetPrimitive( P_LineLoop );
    Prim->SetZTest( true );
    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        for ( int j = 0 ; j < World.Sectors[i].Faces.Length() ; j++ ) {
            FBladeWorld::FFace * f = World.Sectors[i].Faces[j];
            if ( f->Type == FBladeWorld::FT_Portal ) {
                for ( int k = 0 ; k < f->Indices.Length() ; k++ ) {
                    FDVec3 & v = World.Vertices[ f->Indices[k] ];
                    Prim->EmitPoint( v.x*0.001f, -v.y*0.001f, -v.z*0.001f );
                }
                Prim->Flush();
            }
        }
    }
#endif

#ifdef DEBUG_BLADE_GHOST_SECTORS
    Prim->SetPrimitive( P_LineLoop );
    Prim->SetZTest( false );
    for ( int i = 0 ; i < SF.GhostSectors.Length() ; i++ ) {
        FBladeSF::FGhostSector & Sector = SF.GhostSectors[i];

        for ( int j = 0 ; j < Sector.Vertices.Length() ; j++ ) {
            FVec2 & v = Sector.Vertices[ j ];
            Prim->EmitPoint( v.x, Sector.FloorHeight, v.y );
        }
        Prim->Flush();

        for ( int j = 0 ; j < Sector.Vertices.Length() ; j++ ) {
            FVec2 & v = Sector.Vertices[ j ];
            Prim->EmitPoint( v.x, Sector.RoofHeight, v.y );
        }
        Prim->Flush();
    }
#endif

#ifdef DEBUG_BLADE_CAMERA_RECORDS
    FCameraRecord * Record[4];

    Record[0] = amazona_barbaro;
    Record[1] = barbaro_caballero;
    Record[2] = caballero_enano;
    Record[3] = enano_amazona;

    Prim->SetPrimitive( P_LineStrip );
    Prim->SetZTest( false );
    Prim->SetColor(1,1,1,1);
    for ( int r = 0 ; r < 4 ; r++ ) {
        FCameraRecord * Rec = Record[r];
        for ( int i = 0 ; i < Rec->Frames.Length() ; i++ ) {
            FCameraRecord::FFrame & Frame = Rec->Frames[i];
            Prim->EmitPoint( Frame.Position );
        }
        Prim->Flush();
    }
#endif
}

// TODO: ќптимизировать поиск сектора
static int FindSector( const FVec3 & Pos ) {
    bool Inside;
    int SectorIndex = -1;
    int Offset;
    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        FBladeWorld::FSector & Sector = World.Sectors[i];

        Inside = true;
        for ( int f = 0 ; f < Sector.Faces.Length() ; f++ ) {
            FBladeWorld::FFace * Face = Sector.Faces[f];

            Offset = Face->Plane.SideOffset( Pos, 0.0f );
            if ( Offset != EPlaneSide::Front ) {
                Inside = false;
                break;
            }
        }

        if ( Inside ) {
            SectorIndex = i;
            break;
        }
    }
    return SectorIndex;
}

static void DebugCharacterSelection( float _TimeStep ) {
#ifdef DEBUG_CHARACTER_SELECTION
    static FCameraRecord * Record = &barbaro_caballero[0];
    static float Time = 999999.0f;
    static bool PlayState = false;
    static int curplayer = caballero;
    float ts = 3;
    float PlayTimes[4] = {60,50,60,47}; // from selec.py (FrameCamera[])
    static float PlayTime;

    static FSceneNode * TestRecordNode = NULL;
    if ( !TestRecordNode ) {
        TestRecordNode = Scene->CreateChild( "TestRecord" );
    }

    if ( !PlayState ) {
        bool left = Window->IsKeyDown( Key_LEFTARROW );
        bool right = Window->IsKeyDown( Key_RIGHTARROW );
        switch ( curplayer ) {
            case amazona:
                if ( left ) {
                    Record = &enano_amazona[ 1 ]; PlayState = true; Time = 0; curplayer = enano; PlayTime = PlayTimes[enano];
                }
                if ( right ) {
                    Record = &amazona_barbaro[ 0 ]; PlayState = true; Time = 0; curplayer = barbario; PlayTime = PlayTimes[amazona];
                }
                break;
            case barbario:
                if ( left ) {
                    Record = &amazona_barbaro[ 1 ]; PlayState = true; Time = 0; curplayer = amazona; PlayTime = PlayTimes[amazona];
                }
                if ( right ) {
                    Record = &barbaro_caballero[ 0 ]; PlayState = true; Time = 0; curplayer = caballero; PlayTime = PlayTimes[barbario];
                }
                break;
            case caballero:
                if ( left ) {
                    Record = &barbaro_caballero[ 1 ]; PlayState = true; Time = 0; curplayer = barbario; PlayTime = PlayTimes[barbario];
                }
                if ( right ) {
                    Record = &caballero_enano[ 0 ]; PlayState = true; Time = 0; curplayer = enano;  PlayTime = PlayTimes[caballero];
                }
                break;
            case enano:
                if ( left ) {
                    Record = &caballero_enano[ 1 ]; PlayState = true; Time = 0; curplayer = caballero; PlayTime = PlayTimes[caballero];
                }
                if ( right ) {
                    Record = &enano_amazona[ 0 ]; PlayState = true; Time = 0; curplayer = amazona; PlayTime = PlayTimes[enano];
                }
                break;
        }
    }

    if ( PlayState ) {
        int RecFrameIndex = FMath::Floor( Time );

        if ( Time > PlayTime * Record->UnknownFloat ) {
            Time = PlayTime * Record->UnknownFloat;
            RecFrameIndex = FMath::Floor( Time );
            RecFrameIndex = FMath::Min( RecFrameIndex,  Record->Frames.Length() - 1 );
            TestRecordNode->SetPosition( Record->Frames[RecFrameIndex].Position );
            TestRecordNode->SetRotation( Record->Frames[RecFrameIndex].Rotation );
            CameraNode->SetPosition( TestRecordNode->GetPosition() );
            CameraNode->SetRotation( TestRecordNode->GetRotation() );
            PlayState = false;
        } else if ( RecFrameIndex >= Record->Frames.Length()-1 )  {
            RecFrameIndex = Record->Frames.Length() - 1;
            TestRecordNode->SetPosition( Record->Frames[RecFrameIndex].Position );
            TestRecordNode->SetRotation( Record->Frames[RecFrameIndex].Rotation );
            CameraNode->SetPosition( TestRecordNode->GetPosition() );
            CameraNode->SetRotation( TestRecordNode->GetRotation() );
            PlayState = false;
        } else {
            int NextFrameIndex = RecFrameIndex + 1;
            float Lerp = Time - RecFrameIndex;

            const FVec3 & P0 = Record->Frames[RecFrameIndex].Position;
            const FVec3 & P1 = Record->Frames[NextFrameIndex].Position;

            const FQuat & R0 = Record->Frames[RecFrameIndex].Rotation;
            const FQuat & R1 = Record->Frames[NextFrameIndex].Rotation;

            TestRecordNode->SetPosition( FMath::Mix( P0, P1, Lerp ) );
            TestRecordNode->SetRotation( FMath::Slerp( R0, R1, Lerp ) );

            CameraNode->SetPosition( TestRecordNode->GetPosition() );
            CameraNode->SetRotation( TestRecordNode->GetRotation() );

            float Scale = FMath::Mix(Record->Frames[RecFrameIndex].TimeScale, Record->Frames[NextFrameIndex].TimeScale, Lerp );

            Time += _TimeStep*Scale*0.1f*1000;
        }
    }
#endif
}

static void DebugWorldPicking() {
#ifdef DEBUG_WORLD_PICKING
    ImVec2 MousePos = ImGui::GetMousePos();
    FSegment Segment = Camera->RetrieveRay( MousePos.x, MousePos.y );
    FChunkedMeshComponent::FRaycastResult Result;

    bool Intersected = ChunkedMesh->Raycast( Segment.start, Segment.end, Result );
    if ( Intersected ) {

        FVec3 Vec = ( Segment.end - Segment.start ) * Result.Distance;
        float VecLength = FMath::Length( Vec );
        const float SphereRadius = 0.6f;
        FVec3 Pos = Segment.start + ( VecLength > 0.0f ? ( Vec / VecLength ) * ( VecLength - 0.01f ) : FVec3(0.0f) ); // Shift little epsilon

        static FMaterialInstance * PickerMaterial = NULL;
        if ( !PickerMaterial ) {
            FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
            PickerMaterial = Material->CreateInstance();
            //PickerMaterial->Set( PickerMaterial->AddressOf( "SmpBaseColor" ), DefaultTexture );
            FVec4 AmbientColor(1.0f);
            PickerMaterial->Set( PickerMaterial->AddressOf( "AmbientColor" ), AmbientColor );
        }

        static FSceneNode * Mover = NULL;
        if ( !Mover ) {
            FStaticMeshResource * Mesh = GResourceManager->GetResource< FStaticMeshResource >( "*sphere*" );
            Mover = Scene->CreateChild( "Mover" );
            Mover->SetScale( SphereRadius );

            FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
            Material->Load();

            FTextureResource * DefaultTexture = GResourceManager->GetResource< FTextureResource >( "Blade/mipmapchecker.png" );
            FTextureResource::FLoadParameters LoadParameters;
            LoadParameters.SRGB = true;
            LoadParameters.BuildMipmaps = true;
            DefaultTexture->SetLoadParameters( LoadParameters );

            PickerMaterial = Material->CreateInstance();
            PickerMaterial->Set( PickerMaterial->AddressOf( "SmpBaseColor" ), DefaultTexture );

            FStaticMeshComponent * StaticMesh = Mover->CreateComponent< FStaticMeshComponent >();
            StaticMesh->SetMesh( Mesh );
            StaticMesh->SetMaterialInstance( PickerMaterial );
        }
        Mover->SetPosition( Pos );
        int Sector = FindSector( Pos );
        if ( Sector >= 0 ) {
            FVec4 AmbientColor;
            AmbientColor.x = World.Sectors[ Sector ].AmbientColor[0] / 255.0f;
            AmbientColor.y = World.Sectors[ Sector ].AmbientColor[1] / 255.0f;
            AmbientColor.z = World.Sectors[ Sector ].AmbientColor[2] / 255.0f;
            AmbientColor.w = World.Sectors[ Sector ].AmbientIntensity;// * 5;

#ifndef UNLIT
            AmbientColor.w *= 5;
#endif

            PickerMaterial->Set( PickerMaterial->AddressOf( "AmbientColor" ), AmbientColor );
        }
    }
#endif
}

static void DebugKeypress() {
    if ( Window->IsKeyPressed( Key_Y, false ) ) {
        if ( RenderTarget->GetRenderMode() == ERenderMode::SolidTriangles ) {
            RenderTarget->SetRenderMode( ERenderMode::Solid );
        } else {
            RenderTarget->SetRenderMode( ERenderMode::SolidTriangles );
        }
    }
    if ( Window->IsKeyPressed( Key_3, false ) ) {
        if ( RenderTarget->GetAmbientOcclusionMode() != EAmbientOcclusion::OFF ) {
            RenderTarget->SetAmbientOcclusionMode( EAmbientOcclusion::OFF );
        } else {
            RenderTarget->SetAmbientOcclusionMode( EAmbientOcclusion::SSAO16 );
        }
    }
    if ( Window->IsKeyPressed( Key_4, false ) ) {
        RenderTarget->SetColorGradingEnabled( !RenderTarget->IsColorGradingEnabled() );
    }

    if ( Window->IsKeyPressed( Key_5, false ) ) {
        r_spatialCull.ToggleBool();
    }
}

static void UpdateCameraAngles( FMouseMoveEvent & _Event ) {
    static float Yaw = 0.0;
    static float Pitch = 0.0;
    const float RotationSpeed = 0.1f;
    Yaw -= _Event.DeltaX * RotationSpeed;
    Pitch -= _Event.DeltaY * RotationSpeed;
    Pitch = FMath::Clamp( Pitch, -90.0f, 90.0f );
    CameraNode->SetAngles( Pitch, Yaw, 0 );
}

static void UpdateCameraMovement( float _TimeStep ) {
    const float SpeedScale = 5.0f;
    const float Boost = 8.0f;

    float MoveSpeed = _TimeStep * SpeedScale;

    if ( Window->IsKeyDown( Key_LSHIFT ) ) {
        MoveSpeed *= Boost;
    }

    if ( Window->IsKeyDown( Key_W ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetForwardVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_S ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetBackVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_A ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetLeftVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_D ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetRightVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_SPACE ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + FVec3(0,MoveSpeed,0)  );
    }

    if ( Window->IsKeyDown( Key_C ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + FVec3(0,-MoveSpeed,0) );
    }
}

static void DebugSectorPortals( const FBladeWorld::FSector & Sector ) {
#ifdef DEBUG_PORTALS
    static FPrimitiveBatchComponent * DebugPortals = NULL;

    if ( !DebugPortals ) {
        FSceneNode * DebugPortalsNode = Scene->CreateChild( "DebugPortals" );
        DebugPortals = DebugPortalsNode->CreateComponent< FPrimitiveBatchComponent >();
    }

    DebugPortals->Clear();
    DebugPortals->SetZTest( false );

    // Draw sector faces
    for ( int j = 0 ; j < Sector.Faces.Length() ; j++ ) {
        FBladeWorld::FFace & f = *Sector.Faces[ j ];
        if ( f.Type == FBladeWorld::FT_Portal ) {
            continue;
        }
        if ( !f.Vertices.Length() ) {
            // Sky or simple face
            DebugPortals->SetColor( 1,1,0,0.2f);
            DebugPortals->SetPrimitive( P_TriangleFan );
            for ( int k = f.Indices.Length()-1 ; k >=0  ; k-- ) {
                FDVec3 & v = World.Vertices[ f.Indices[ k ] ];

                DebugPortals->EmitPoint( v.x*0.001f, -v.y*0.001f, -v.z*0.001f );
            }
        } else {
            // Face with holes
            DebugPortals->SetPrimitive( P_Triangles );
            DebugPortals->SetColor( 1,1,1,0.2f);
            for ( int k = f.Indices.Length()-1 ; k >=0  ; k-- ) {
                FDVec3 & v = f.Vertices[ f.Indices[ k ] ];

                DebugPortals->EmitPoint( v.x*0.001f, -v.y*0.001f, -v.z*0.001f );
            }
        }
        DebugPortals->Flush();
    }

    // Draw sector portals
    for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
        FBladeWorld::FPortal * Portal = Sector.Portals[j];
        DebugPortals->SetColor( 0,0,1,0.2f);
        DebugPortals->SetPrimitive( P_TriangleFan );
        for ( int k = 0 ; k < Portal->Winding.Length() ; k++ ) {
            FDVec3 & v = Portal->Winding[ k ];
            DebugPortals->EmitPoint( v.x*0.001f, -v.y*0.001f, -v.z*0.001f );
        }
        DebugPortals->Flush();
    }

    //DebugPortals->SetPrimitive( P_LineLoop );
    //for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
    //    FBladeWorld::FFace & f = *Sector.Portals[ j ].Face;
    //    for ( int k = 0 ; k < f.Indices.Length() ; k++ ) {
    //        FDVec3 & v = World.Vertices[ f.Indices[ k ] ];
    //        DebugPortals->EmitPoint( v.x*0.001f, -v.y*0.001f, -v.z*0.001f );
    //    }
    //    DebugPortals->Flush();
    //}
#endif
}

void FGame::OnBeforeCommandLineProcessing( TMutableArray< char * > & _Arguments ) {
    _Arguments.Append( "-splash" );
    _Arguments.Append( "Blade/splash.png" );
    _Arguments.Append( "-splashOpacity" );
    _Arguments.Append( "0.8" );
}

void FGame::OnSetPrimaryWindowDefs( FWindowDefs & _WindowDefs ) {
    _WindowDefs.Title = "Blade Of Darkness Remake. Powered by Angie Engine";
    _WindowDefs.AutoIconify = true;
    _WindowDefs.Focused = true;
}

void FGame::OnInitialize() {
    LoadConfigFile();

    Scene = TNew< FScene >();

    Scene->GetOrCreateComponent< FWorldComponent >();
    Scene->GetOrCreateComponent< FPhysicsComponent >();

    //FBladeCSV CSV;
    //CSV.LoadCSV( MakePath( "Maps/csv.dat" ) );

    amazona_barbaro[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_amazona_barbaro.cam" ) );
    amazona_barbaro[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_barbaro_amazona.cam" ) );

    barbaro_caballero[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_barbaro_caballero.cam" ) );
    barbaro_caballero[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_caballero_barbaro.cam" ) );

    caballero_enano[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_caballero_enano.cam" ) );
    caballero_enano[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_enano_caballero.cam" ) );

    enano_amazona[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_enano_amazona.cam" ) );
    enano_amazona[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_amazona_enano.cam" ) );

    //LoadLevel( MakePath( "Maps/Casa/casa.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Casa/casa.sf" ) );

    // Ётот уровень загружаетс€ с ошибками
    //LoadLevel( MakePath( "Maps/Mine_M5/mine.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Mine_M5/mine.sf" ) );

    //LoadLevel( MakePath( "Maps/Chaos_M17/chaos.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Chaos_M17/ambiente.sf" ) );

    //LoadLevel( MakePath( "Maps/Btomb_M12/btomb.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Btomb_M12/ambiente.sf" ) );

    //LoadLevel( MakePath( "Maps/Island_M8/island.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Island_M8/island.sf" ) );

    //LoadLevel( MakePath( "Maps/Labyrinth_M6/labyr.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Labyrinth_M6/sonidos.sf" ) );

    //LoadLevel( MakePath( "Maps/Dwarf_M3/dwarf.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Dwarf_M3/dwarf.sf" ) );

    //LoadLevel( MakePath( "Maps/Ice_M11/ice.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Ice_M11/ice.sf" ) );

    //LoadLevel( MakePath( "Maps/Volcano_M14/volcano.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Volcano_M14/volcano.sf" ) );

    //LoadLevel( MakePath( "Maps/Orlok_M10/defile.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Orlok_M10/exterior.sf" ) );

    //LoadLevel( MakePath( "Maps/Tower_M16/tower.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Tower_M16/tower.sf" ) );

    //LoadLevel( MakePath( "Maps/Ragnar_M2/ragnar.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Ragnar_M2/ragnar.sf" ) );

    //LoadLevel( MakePath( "Maps/Tutorial/tutorial.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Tutorial/tutorial.sf" ) );

    //LoadLevel( MakePath( "Maps/Desert_back/desert.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Desert_back/desert.sf" ) );

    //LoadLevel( MakePath( "Maps/Barb_M1/barb.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Barb_M1/barb.sf" ) );

    //LoadLevel( MakePath( "Maps/Orc_M9/orcst.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Orc_M9/orc.sf" ) );

    //LoadLevel( MakePath( "Maps/Tomb_M7/tomb.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Tomb_M7/tomb.sf" ) );

    //LoadLevel( MakePath( "Maps/Ruins_M4/ruins.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Ruins_M4/ruinssnd.sf" ) );

    FString SFName = demo_gamelevel.GetString();
    SFName.ReplaceExt( ".sf" );

    LoadLevel( MakePath( demo_gamelevel.GetString() ) );
    LoadGhostSectors( MakePath( SFName.Str() ) );
    LoadMusic();
    CreateAreasAndPortals();
    CreateCamera();
    CreateSunLight();
    CreateWorldGeometry();
    CreateDebugMesh();

    Scene->SetDebugDrawFlags( 0 );

    RenderTexture = GResourceManager->CreateUnnamedResource< FTextureResource >();
    RenderTexture->CreateRenderTarget();
    RenderTarget = RenderTexture->GetRenderTarget();
    RenderTarget->SetEraseBackground( true );
    RenderTarget->SetBackgroundColor( FVec4(0.4f,0.4f,1.0f,1) );
    RenderTarget->SetWireframeColor( FVec4(1) );
    RenderTarget->SetWireframeLineWidth( 2.0f );
    RenderTarget->SetRenderMode( ERenderMode::Solid );
    RenderTarget->SetEraseDepth( true );
    RenderTarget->SetViewCamera( Camera );
    RenderTarget->SetCullCamera( Camera );
    RenderTarget->SetColorGradingEnabled( demo_colorgrading.GetBool() );
    RenderTarget->SetColorGradingGamma( FVec3( 0.624f ) );
    RenderTarget->SetColorGradingLift( FVec3( 0.472f ) );
    RenderTarget->SetColorGradingPresaturation( FVec3( 1.0f ) );
    RenderTarget->SetColorGradingTemperature( 4601.678f );
    RenderTarget->SetColorGradingTemperatureStrength( FVec3( 1.0f ) );
    RenderTarget->SetColorGradingBrightnessNormalization( 0.0f );

    RenderTarget->SetAmbientOcclusionMode( demo_ssao.GetBool() ? EAmbientOcclusion::SSAO16 : EAmbientOcclusion::OFF );
    if ( SkyboxTexture ) {
        RenderTarget->EnvProbeDebug = GResourceManager->CreateUnnamedResource< FTextureResource >();
        //RenderTarget->EnvProbeDebug->GenerateEnvProbe( 7, GResourceManager->GetResource<FTextureResource>("*black*") );
        RenderTarget->EnvProbeDebug->GenerateEnvProbe( 7, SkyboxTexture );
        //RenderTarget->EnvProbeDebug->GenerateEnvProbe( 1, SkyboxTexture );
    }

    GPlatformPort->CloseSplashScreen();

    Window = GetPrimaryWindow();
    Window->SetTexture( RenderTexture );
    if ( !demo_fullscreen.GetBool() ) {
        FMonitor * Monitor = GPlatformPort->GetPrimaryMonitor();
        int w = Monitor->Modes[Monitor->CurrentVideoMode].Width;
        int h = Monitor->Modes[Monitor->CurrentVideoMode].Height;
        Window->GetPlatformWindow()->SetPosition( (w - demo_width.GetInteger())/2, (h - demo_height.GetInteger())/2 );
        Window->GetPlatformWindow()->Resize( demo_width.GetInteger(), demo_height.GetInteger() );
    } else {
        Window->GetPlatformWindow()->SetMonitor( GPlatformPort->GetPrimaryMonitor(), 0, 0, demo_width.GetInteger(), demo_height.GetInteger(), 120 );
    }
    Window->GetPlatformWindow()->SetVisible(true);
    Window->E_OnKeyPress.Subscribe( this, &FGame::OnKeyPress );
    Window->E_OnKeyRelease.Subscribe( this, &FGame::OnKeyRelease );
    Window->E_OnChar.Subscribe( this, &FGame::OnChar );
    Window->E_OnMouseMove.Subscribe( this, &FGame::OnMouseMove );
    Window->E_OnUpdateGui.Subscribe( this, &FGame::OnUpdateGui );
    Window->SetCursorMode( ECursorMode::Disabled );
    Window->SetSwapControlMode( ESwapControl::Adaptive );

    ImGui_Create( Window );
}

void FGame::OnShutdown() {
    Scene.Reset();

    ImGui_Release();
}

void FGame::OnKeyPress( FKeyPressEvent & _Event ) {
    ImGui_KeyPress( _Event );

    if ( _Event.KeyCode == Key_ESCAPE ) {
        Terminate();
    }
}

void FGame::OnKeyRelease( FKeyReleaseEvent & _Event ) {
    ImGui_KeyRelease( _Event );
}

void FGame::OnChar( FCharEvent & _Event ) {
    ImGui_Char( _Event );
}

void FGame::OnMouseMove( FMouseMoveEvent & _Event ) {
    ImGui_MouseMove( _Event );

    UpdateCameraAngles( _Event );
}

void FGame::OnUpdateGui( FUpdateGuiEvent & _Event ) {
    ImGui_BeginUpdate();

    ImGui::SetMouseCursor( ImGuiMouseCursor_None );

#if 0
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize( ImVec2(650,150) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    if ( ImGui::Begin( "Background", NULL, ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoTitleBar ) ) {

        ImGui::SetWindowFontScale( 1.5f );
        ImGui::Text( "Powered by Angie Engine (C)" );
        ImGui::Text( "Programmed by Alexander Samusev (0xc0de)" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://vk.com/bladeremake" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://twitter.com/BladeRemake" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://www.youtube.com/channel/UC7EK28QMYfy1Ke6tan6pTwg" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://alsamusev-lostlands.blogspot.ru" );
    }
    ImGui::End();
    ImGui::PopStyleVar();
#endif
#if 1
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize( ImVec2(RenderTexture->GetWidth(),RenderTexture->GetHeight()) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4(0,0,0,0) );
    if ( ImGui::Begin( "Background", NULL, ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoTitleBar ) ) {

        ImGui::SetWindowFontScale( 4 );
        if ( r_spatialCull.GetBool() ) {
            ImGui::Text( "VSD ON" );
        } else {
            ImGui::Text( "VSD OFF" );
        }        
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
#endif
#if 0
    if ( ImGui::Begin( "Options" ) ) {

        static float MasterGain = GAudioSystem->GetMasterGain();
        if ( ImGui::SliderFloat( "Mater Gain", &MasterGain, 0, 1 ) ) {
            GAudioSystem->SetMasterGain( MasterGain );
        }

        static float MusicVolume = 1.0f;
        static float SoundVolume = 1.0f;

        if ( ImGui::SliderFloat( "Music Volume", &MusicVolume, 0, 1 ) ) {
            FAudioComponent * ac = Scene->GetComponent< FAudioComponent >();
            ac->SetGainScale( SND_MUSIC, MusicVolume );
        }

        if ( ImGui::SliderFloat( "Sound Volume", &SoundVolume, 0, 1 ) ) {
            FAudioComponent * ac = Scene->GetComponent< FAudioComponent >();
            ac->SetGainScale( SND_SOUND, SoundVolume );
        }

        static float Brightness = 0.5f;
        if ( ImGui::SliderFloat( "Brightness", &Brightness, 0.0f, 1.0f ) ) {
            RenderTarget->SetBrightness( Brightness );
        }

        static float Gamma = RenderTarget->GetColorGradingGamma().x;
        if ( ImGui::DragFloat( "Gamma", &Gamma, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingGamma( FVec3(Gamma) );
        }

        static float Lift = RenderTarget->GetColorGradingLift().x;
        if ( ImGui::DragFloat( "Lift", &Lift, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingLift( FVec3(Lift) );
        }

        static float Presat = RenderTarget->GetColorGradingPresaturation().x;
        if ( ImGui::DragFloat( "Presat", &Presat, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingPresaturation( FVec3(Presat) );
        }

        static float Temperature = RenderTarget->GetColorGradingTemperature();
        if ( ImGui::DragFloat( "Temperature", &Temperature, 50.f, 1000.0f, 40000.0f ) ) {
            RenderTarget->SetColorGradingTemperature( Temperature );
        }

        static float Strength = RenderTarget->GetColorGradingTemperatureStrength().x;
        if ( ImGui::DragFloat( "Strength", &Strength, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingTemperatureStrength( FVec3(Strength) );
        }

        static float BrightNorm = RenderTarget->GetColorGradingBrightnessNormalization();
        if ( ImGui::DragFloat( "BrightNorm", &BrightNorm, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingBrightnessNormalization( BrightNorm );
        }
    
#ifdef DEBUG_MONITOR_GAMMA
        FGammaRamp Ramp;
        static unsigned short GammaRamp[256];
        Ramp.Red = GammaRamp;
        Ramp.Green = GammaRamp;
        Ramp.Blue = GammaRamp;
        Ramp.Size = 256;

        bool UpdateGamma = false;
        
        static float Gamma = 1.0f;
        if ( ImGui::SliderFloat( "Gamma", &Gamma, 0.5f, 3.0f ) ) {
            UpdateGamma = true;
        }

        static float Brightness = 0.5f;
        if ( ImGui::SliderFloat( "Brightness", &Brightness, 0.5f, 100.0f ) ) {
            UpdateGamma = true;
        }

        static float Overbright = 0.0f;
        if ( ImGui::SliderFloat( "Overbright", &Overbright, 0.0f, 10.0f ) ) {
            UpdateGamma = true;
        }

        for ( int i = 0 ; i < 256 ; i++ ) {
            float f = ( 255.0f * pow( i / 255.0f, 1.0f / Gamma ) + Brightness ) * ( Overbright + 1 );

            f = FMath::Clamp( f, 0.0f, 255.0f );

            GammaRamp[ i ] = ( unsigned short )( (int)( f ) << 8 );
        }

        if ( UpdateGamma ) {
            GPlatformPort->SetGammaRamp( *GPlatformPort->GetPrimaryMonitor(), &Ramp );
        }
#endif
    }
    ImGui::End();
#endif
    ImGui_EndUpdate();
}

void FGame::OnUpdate( float _TimeStep ) {
    static int PrevSectorIndex = -1;

    DebugKeypress();
    DebugCharacterSelection( _TimeStep );
    DebugWorldPicking();
    UpdateCameraMovement( _TimeStep );

    int SectorIndex = FindSector( CameraNode->GetPosition() );
    if ( SectorIndex >= 0 && SectorIndex != PrevSectorIndex ) {
        FBladeWorld::FSector & Sector = World.Sectors[SectorIndex];

        PrevSectorIndex = SectorIndex;

        // Update color grading inside the sector
        FVec3 Color;
        byte YCoCg[3], RGB[3];
        FColorSpace::RGBToYCoCg( Sector.AmbientColor, YCoCg );
        YCoCg[0] = 128;//255;
        FColorSpace::YCoCgToRGB( YCoCg, RGB );
        Color.r = RGB[0] / 255.0f;
        Color.g = RGB[1] / 255.0f;
        Color.b = RGB[2] / 255.0f;
        RenderTarget->SetColorGradingGrain( Color );
        RenderTarget->SetColorGradingLUT( NULL );

        // Draw some debug info
        DebugSectorPortals( Sector );
    }

    Scene->Update( NULL, Camera, _TimeStep );  
}

AN_APP_ENTRY_DECL( FGame )
