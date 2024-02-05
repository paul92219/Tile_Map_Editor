/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Paul Solodrai $
   ======================================================================== */

#include "editor.h"
#include "editor_sort.cpp"
#include "editor_render.cpp"
#include "editor_render_group.cpp"
#include "editor_world.cpp"
#include "editor_sim_region.cpp"
#include "editor_asset.cpp"

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY,
                              v2 AdditionalOffset = V2(0, 0))
{
    world_position BasePos = {};

    real32 TileSideInMeters = 1.0f;
    
    v2 TileDim = V2(TileSideInMeters, TileSideInMeters);
    v2 Offset = Hadamard(TileDim, V2((real32)AbsTileX, (real32)AbsTileY));
    world_position Result = MapIntoChunkSpace(World, BasePos, AdditionalOffset + Offset);
    
    Assert(IsCanonical(World, Result.Offset_));
    
    return(Result);
}

#include "editor_text_and_cursors.cpp"
#include "editor_terrain_mode.cpp"
#include "editor_decoration_mode.cpp"

internal void
ClearBitmap(loaded_bitmap *Bitmap)
{
    if(Bitmap->Memory)
    {
        int32 TotalBitmapSize = Bitmap->Width*Bitmap->Height*BITMAP_BYTES_PER_PIXEL;
        ZeroSize(TotalBitmapSize, Bitmap->Memory);
    }
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *Arena, int32 Width, int32 Height, bool32 ClearToZero = true)
{
    loaded_bitmap Result = {};

    Result.Width = Width;
    Result.Height = Height;
    Result.AlignPercentage = V2(0.5f, 0.5f);
    Result.WidthOverHeight = SafeRatio0((real32)Result.Width, (real32)Result.Height);
    
    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    int32 TotalBitmapSize = Width*Height*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, TotalBitmapSize, Align(16, ClearToZero));
    if(ClearToZero)
    {
        ClearBitmap(&Result);
    }

    return(Result);
}

internal task_with_memory *
BeginTaskWithMemory(transient_state *TranState)
{
    task_with_memory *FoundTask = 0;
    for(uint32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        ++TaskIndex)
    {
        task_with_memory *Task = TranState->Tasks + TaskIndex;
        if(!Task->BeingUsed)
        {
            Task->BeingUsed = true;
            FoundTask = Task;
            Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
            break;
        }
    }

    return(FoundTask);
}

internal void
EndTaskWithMemory(task_with_memory *Task)
{
    EndTemporaryMemory(Task->MemoryFlush);

    CompletePreviousWritesBeforeFutureWrites;
    Task->BeingUsed = false;
}

internal void
FillGroundChunk(transient_state *TranState, game_state *GameState, ground_buffer *GroundBuffer,
                world_position *ChunkP, real32 TileSideInMeters)
{
    temporary_memory GroundMemory = BeginTemporaryMemory(&TranState->TranArena);

    loaded_bitmap *Buffer = &GroundBuffer->Bitmap;
    Buffer->AlignPercentage = V2(0.5f, 0.5f);
    Buffer->WidthOverHeight = 1.0f; 

    real32 Width = GameState->World->ChunkDimInMeters.x;
    real32 Height = GameState->World->ChunkDimInMeters.y;
    Assert(Width == Height);
    v2 HalfDim = 0.5f*V2(Width, Height);

    u32 PushBufferSize = Megabytes(4);
    void *PushBuffer = Platform.AllocateMemory(PushBufferSize);
    void *SortMemory = Platform.AllocateMemory(PushBufferSize);
    void *ClipRectMemory = Platform.AllocateMemory(PushBufferSize);
    game_render_commands Commands = RenderCommandStruct(PushBufferSize, PushBuffer, 
                                                              (u32)Buffer->Width,
                                                              (u32)Buffer->Height);
//    render_group RenderGroup = BeginRenderGroup(TranState->Assets, &Commands, TranState->MainGenerationID, true);
//    Orthographic(&RenderGroup, Buffer->Width, Buffer->Height, Buffer->Width / Width);
//    Clear(&RenderGroup, V4(1.0f, 0.0f, 1.0f, 1.0f));
    object_transform Transform = DefaultFlatTransform();
    GroundBuffer->P = *ChunkP;

    int32 TileCountX = WORLD_WIDTH_TILE_COUNT;
    int32 TileCountY = WORLD_HEIGHT_TILE_COUNT;

    if((ChunkP->ChunkX >= 0) && (ChunkP->ChunkY >= 0))
    {
        int32 MinTileX = TILES_PER_CHUNK*ChunkP->ChunkX;
        int32 MaxTileX = MinTileX + TILES_PER_CHUNK;
        int32 MinTileY = TILES_PER_CHUNK*ChunkP->ChunkY;
        int32 MaxTileY = MinTileY + TILES_PER_CHUNK;

        for(int32 TileY = MinTileY;
            TileY < MaxTileY;
            ++TileY)
        {
            for(int32 TileX = MinTileX;
                TileX < MaxTileX;
                ++TileX)
            {
//                if((TileX < TileCountX) && (TileY < TileCountY))
                if((TileX < 8) && (TileY < 8))
                {
                    world_tile *WorldTile = GameState->WorldTiles + TileY*TileCountX + TileX;

                    real32 X = (real32)(TileX - MinTileX);
                    real32 Y = (real32)(TileY - MinTileY);
                    v2 P = -HalfDim + V2(TileSideInMeters*X, TileSideInMeters*Y);
                    
                    bitmap_id ID = WorldTile->TileBitmapID;
//                    PushBitmap(&RenderGroup, Transform, ID, 1.0f, V3(P, 0));
                }
            }
        }
    }

//    Platform.DeallocateMemory(PushBuffer);
    SortEntries(&Commands, SortMemory);
    LinearizeClipRects(&Commands, ClipRectMemory);

//    SoftwareRenderCommands(TranState->HighPriorityQueue, &Commands, Buffer);        
//    EndRenderGroup(&RenderGroup);
    EndTemporaryMemory(GroundMemory);
}

internal void
InitializeWorldTilesAndDecorations(game_assets *Assets, game_state *GameState,
                                   char *TilesFileName, char *DecorationsFileName)
{
    debug_read_file_result WorldTiles = Platform.DEBUGReadEntireFile(0, TilesFileName);
    if(WorldTiles.Contents)
    {
        GameState->WorldTiles = (world_tile *)WorldTiles.Contents;
        for(u32 TileIndex = 0;
            TileIndex < GameState->WorldTileCount;
            ++TileIndex)
        {
            world_tile *Tile = GameState->WorldTiles + TileIndex;
            GameState->TileIDs[TileIndex] = Tile->TileID;
            Tile->TileBitmapID = GetBitmapForTile(Assets, GlobalTilesetInfo, GlobalTileset, Tile->TileID);
        }
    }
    else
    {
        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        WeightVector.E[Tag_TileID] = 1.0f;
        MatchVector.E[Tag_TileID] = 3.0f;
        bitmap_id TileBitmapID = GetBestMatchBitmapFrom(Assets, Asset_Tile,
                                                        &MatchVector, &WeightVector);
        for(u32 TileIndex = 0;
            TileIndex < GameState->WorldTileCount;
            ++TileIndex)
        {
            world_tile *WorldTile = GameState->WorldTiles + TileIndex;
            GameState->TileIDs[TileIndex] = 3;
            WorldTile->TileID = 3;
            WorldTile->TileBitmapID = TileBitmapID;
        }
    }

    debug_read_file_result Decorations = Platform.DEBUGReadEntireFile(0, DecorationsFileName);
    if(Decorations.Contents)
    {
        GameState->Decorations = (decoration *)Decorations.Contents;
    }

    GameState->WorldTilesInitialized = true;
}

inline world_tile *
GetTileFromChunkPosition(game_state *GameState, world_position *MouseP)
{
    world_tile *Result = 0;
    u32 WorldTileIndex = GetTileIndexFromChunkPosition(GameState, MouseP);

    Result = GameState->WorldTiles + WorldTileIndex;
    
    return(Result);
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{    
    Platform = Memory->PlatformAPI;

    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));

    uint32 GroundBufferWidth = 256; 
    uint32 GroundBufferHeight = 256;

    uint32 TileSideInPixels = 32;
    real32 PixelsToMeters = 1.0f / TileSideInPixels;
    real32 TileSideInMeters = TileSideInPixels * PixelsToMeters;

    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if(!Memory->IsInitialized)
    {
        uint32 TilesPerWidth = 30;
        uint32 TilesPerHeight = 17;

        v2 WorldChunkDimInMeters =
            {
                TILES_PER_CHUNK * TileSideInMeters,
                TILES_PER_CHUNK * TileSideInMeters
            };

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));


        GameState->WorldTileCount = WORLD_HEIGHT_TILE_COUNT*WORLD_WIDTH_TILE_COUNT;
        GameState->WorldTiles = PushArray(&GameState->WorldArena, GameState->WorldTileCount, world_tile);
        GameState->Decorations = PushArray(&GameState->WorldArena, GameState->WorldTileCount, decoration);
        GameState->TileIDs = PushArray(&GameState->WorldArena, GameState->WorldTileCount, u32);
        
        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        InitializeWorld(World, WorldChunkDimInMeters);
        
        uint32 ScreenBaseX = 0;
        uint32 ScreenBaseY = 0;
        uint32 ScreenX = ScreenBaseX;
        uint32 ScreenY = ScreenBaseY;
        for(uint32 ScreenIndex = 0;
            ScreenIndex < 48;
            ++ScreenIndex)
        {
            for(uint32 TileY = 0;
                TileY < TilesPerHeight;
                ++TileY)
            {
                for(uint32 TileX = 0;
                    TileX < TilesPerWidth;
                    ++TileX)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;
                }
            }

            if(ScreenX < 7)
            {
                ++ScreenX;
            }
            else
            {
                ScreenX = 0;
            }

            if((ScreenY < 5) && (ScreenX == 0))
            {
                ++ScreenY;
            }
        }
        
        world_position NewCameraP = {};
        GameState->CameraP = NewCameraP;

        GameState->CameraBoundsMin.ChunkX = 0;
        GameState->CameraBoundsMin.ChunkY = 0;
    
        GameState->CameraBoundsMax.ChunkX = WORLD_WIDTH_TILE_COUNT / TILES_PER_CHUNK;
        GameState->CameraBoundsMax.ChunkY = WORLD_HEIGHT_TILE_COUNT / TILES_PER_CHUNK;

        // NOTE(paul): Initialize cursors and sets stats
        InitializeCursor(&GameState->TileMenuBarCursor, 10);
        InitializeCursor(&GameState->AssetMenuBarCursor, 2);

        GameState->TileSetStats = {};
        GameState->AssetSetStats = {};
        GameState->AssetSetStats.Type = Asset_Bole;
        
        Memory->IsInitialized = true;
    }


    // NOTE(casey): Transient initialization
    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);    
    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(!TranState->IsInitialized)
    {
        InitializeArena(&TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
                        (uint8 *)Memory->TransientStorage + sizeof(transient_state));

        TranState->HighPriorityQueue = Memory->HighPriorityQueue;
        TranState->LowPriorityQueue = Memory->LowPriorityQueue;
        
        for(uint32 TaskIndex = 0;
            TaskIndex < ArrayCount(TranState->Tasks);
            ++TaskIndex)
        {
            task_with_memory *Task = TranState->Tasks + TaskIndex;

            Task->BeingUsed = false;
            SubArena(&Task->Arena, &TranState->TranArena, Megabytes(1));
        }

        TranState->Assets = AllocateGameAssets(&TranState->TranArena, Megabytes(16), TranState);
        
        TranState->GroundBufferCount = 256;
        TranState->GroundBuffers = PushArray(&TranState->TranArena, TranState->GroundBufferCount, ground_buffer);
        for(uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            ++GroundBufferIndex)
        {
            ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->Bitmap = MakeEmptyBitmap(&TranState->TranArena, GroundBufferWidth, GroundBufferHeight, false);
            GroundBuffer->P = NullPosition();
        }

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        WeightVector.E[Tag_BiomeType] = 1.0f;
        WeightVector.E[Tag_TileMainSurface] = 1.0f;
        WeightVector.E[Tag_TileMergeSurface] = 1.0f;

        MatchVector.E[Tag_BiomeType] = (r32)BiomeType_Global;
        MatchVector.E[Tag_TileMainSurface] = (r32)TileSurface_Global;
        MatchVector.E[Tag_TileMergeSurface] = (r32)TileSurface_Global;
        tileset_id ID = GetBestMatchTilesetFrom(TranState->Assets, Asset_Tileset,
                                                &MatchVector, &WeightVector);

        GameState->GlobalTilesetID = ID;
        
        TranState->IsInitialized = true;
    }

    if(TranState->MainGenerationID)
    {
        EndGeneration(TranState->Assets, TranState->MainGenerationID);
    }
    TranState->MainGenerationID = BeginGeneration(TranState->Assets);
    
    
    if(Input->ExecutableReloaded)
    {
        ResetGroundBuffers(TranState);
    }

    world *World = GameState->World;

    //
    // NOTE(casey): 
    //

    game_controller_input *Controller = GetController(Input, 0);
    if(Controller->IsAnalog)
    {
        // NOTE(casey): Use analog movement tuning
    }
    else
    {
        v2 NewP = {};
        // NOTE(casey): Use digital movement tuning
        if(Controller->MoveUp.EndedDown)
        {
            NewP.y += 4.0f;
        }
        if(Controller->MoveDown.EndedDown)
        {
            NewP.y -= 4.0f;
        }
        if(Controller->MoveLeft.EndedDown)
        {
            NewP.x -= 4.0f;
        }
        if(Controller->MoveRight.EndedDown)
        {
            NewP.x += 4.0f;
        }
        
        GameState->CameraP = MapIntoChunkSpace(World, GameState->CameraP, NewP);

        if(Controller->ChangeEditMode.EndedDown)
        {
            GameState->EditMode += 1;
            if(GameState->EditMode >= EditMode_Count)
            {
                GameState->EditMode = 0;
            }
        }

        if(GameState->EditMode == EditMode_Terrain)
        {
            if(Controller->Biome.EndedDown)
            {
                GameState->TileSetStats.Biome += 1;
                if(GameState->TileSetStats.Biome >= BiomeType_Count)
                {
                    GameState->TileSetStats.Biome = 0;
                }
            }

            if(Controller->Type.EndedDown)
            {
                GameState->TileSetStats.Type += 1;
                if(GameState->TileSetStats.Type >= TileType_Count)
                {
                    GameState->TileSetStats.Type = 0;
                }
            }

            if(Controller->Height.EndedDown)
            {
                GameState->TileSetStats.Height += 1;
                if(GameState->TileSetStats.Height >= Height_Count)
                {
                    GameState->TileSetStats.Height = 0;
                }
            }

            if(Controller->CliffHillType.EndedDown)
            {
                GameState->TileSetStats.CliffHillType += 1;
                if(GameState->TileSetStats.CliffHillType >= CliffHillType_Count)
                {
                    GameState->TileSetStats.CliffHillType = 0;
                }
            }

            if(Controller->MainSurface.EndedDown)
            {
                GameState->TileSetStats.MainSurface += 1;
                if(GameState->TileSetStats.MainSurface >= TileSurface_Count)
                {
                    GameState->TileSetStats.MainSurface = 0;
                }
            }
            if(Controller->MergeSurface.EndedDown)
            {
                GameState->TileSetStats.MergeSurface += 1;
                if(GameState->TileSetStats.MergeSurface >= TileSurface_Count)
                {
                    GameState->TileSetStats.MergeSurface = 0;
                }
            }
        }
        else if(GameState->EditMode == EditMode_Decoration)
        {
            if(Controller->Biome.EndedDown)
            {
                GameState->AssetSetStats.Biome += 1;
                if(GameState->AssetSetStats.Biome >= BiomeType_Count)
                {
                    GameState->AssetSetStats.Biome = 0;
                }
            }

            if(Controller->Type.EndedDown)
            {
                GameState->AssetSetStats.Type += 1;
                if(GameState->AssetSetStats.Type >= Asset_Count)
                {
                    GameState->AssetSetStats.Type = 0;
                }
            }
        }

        if(Controller->ActionUp.EndedDown)
        {
//            ChangeChoosenAttributeValueFor(GameState, true);
        }
        if(Controller->ActionDown.EndedDown)
        {
//            ChangeChoosenAttributeValueFor(GameState, false);
        }
    }
    
    //
    // NOTE(casey): Render
    //

    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);
    
    loaded_bitmap DrawBuffer = {};
    DrawBuffer.Width = RenderCommands->Width;
    DrawBuffer.Height = RenderCommands->Height;

    render_group TextRenderGroup = BeginRenderGroup(TranState->Assets, RenderCommands,
                                                     TranState->MainGenerationID, false);
    Orthographic(&TextRenderGroup, DrawBuffer.Width, DrawBuffer.Height, 1.0f);


    // TODO(casey): Decide what our pushbuffer size is!
    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands,
                                                 TranState->MainGenerationID, false);
    render_group *RenderGroup = &RenderGroup_;
    real32 WidthOfMonitor = 0.635f; // NOTE(casey): Horizontal measurement of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer.Width*WidthOfMonitor;// / 2.0f;
    real32 FocalLength = 0.4f;
    real32 DistanceAboveTarget = 9.0f;
    Perspective(RenderGroup, DrawBuffer.Width, DrawBuffer.Height, MetersToPixels, FocalLength, DistanceAboveTarget);


    // NOTE(paul): Load GlobalTileset
    GlobalTileset = PushTileset(RenderGroup, GameState->GlobalTilesetID, true);
    GlobalTilesetInfo = GetTilesetInfo(TranState->Assets, GameState->GlobalTilesetID);
    if(!GameState->WorldTilesInitialized)
    {
        InitializeWorldTilesAndDecorations(TranState->Assets, GameState, "worldtiles.bin", "decorations.bin");
    }
    
    // NOTE(paul): Reset font spacing
    DEBUGReset(&TextRenderGroup, DrawBuffer.Width, DrawBuffer.Height);

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));

    v2 ScreenCenter = {0.5f*(real32)DrawBuffer.Width,
                       0.5f*(real32)DrawBuffer.Height};

    rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
    rectangle2 CameraBoundsInMeters = RectMinMax(ScreenBounds.Min, ScreenBounds.Max);

    v2 SimBoundsExpansion = {15.0f, 14.0f};
    rectangle2 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
    
    char TextBuffer[256];
    _snprintf_s(TextBuffer, sizeof(TextBuffer),
                "Edit Mode: %s",
                EditModeText[GameState->EditMode]);
    DEBUGTextLine(&TextRenderGroup, TextBuffer);
    
    _snprintf_s(TextBuffer, sizeof(TextBuffer),
                "CameraP in Chunks: X: %d Y: %d, OX: %f, OY: %f",
                GameState->CameraP.ChunkX, GameState->CameraP.ChunkY,
                GameState->CameraP.Offset_.x, GameState->CameraP.Offset_.y);
    DEBUGTextLine(&TextRenderGroup, TextBuffer);

    object_transform Transform = DefaultUprightTransform();    

    r32 MouseX = (r32)Input->MouseX;
    r32 MouseY = (r32)Input->MouseY;
    v2 P = Unproject(RenderGroup, Transform, V2(MouseX, MouseY)).xy;
    world_position MouseChunkP = MapIntoChunkSpace(World, GameState->CameraP, P);

    _snprintf_s(TextBuffer, sizeof(TextBuffer),
                "MouseP in Chunks: X: %d Y: %d, OX: %f, OY: %f",
                MouseChunkP.ChunkX, MouseChunkP.ChunkY,
                MouseChunkP.Offset_.x, MouseChunkP.Offset_.y);
    DEBUGTextLine(&TextRenderGroup, TextBuffer);

    tile_position MouseTileP = TilePositionFromChunkPosition(&MouseChunkP);
    _snprintf_s(TextBuffer, sizeof(TextBuffer),
                "MouseP in Tiles: X: %d Y: %d",
                MouseTileP.TileX, MouseTileP.TileY);
    DEBUGTextLine(&TextRenderGroup, TextBuffer);

    // NOTE(casey): Ground chunk updating
    {
        world_position MinChunkP = MapIntoChunkSpace(World, GameState->CameraP, GetMinCorner(CameraBoundsInMeters));
        world_position MaxChunkP = MapIntoChunkSpace(World, GameState->CameraP, GetMaxCorner(CameraBoundsInMeters));
        for(int32 ChunkY = MinChunkP.ChunkY;
            ChunkY <= MaxChunkP.ChunkY;
            ++ChunkY)
        {
            for(int32 ChunkX = MinChunkP.ChunkX;
                ChunkX <= MaxChunkP.ChunkX;
                ++ChunkX)
            {
                world_position ChunkCenterP = CenteredChunkPoint(ChunkX, ChunkY);
                v2 RelP = Subtract(World, &ChunkCenterP, &GameState->CameraP);

                // TODO(casey): This is super inefficient fix it!
                real32 FurthestBufferLengthSq = 0.0f;
                ground_buffer *FurthestBuffer = 0;
                for(uint32 GroundBufferIndex = 0;
                    GroundBufferIndex < TranState->GroundBufferCount;
                    ++GroundBufferIndex)
                {
                    ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
                    if(AreInSameChunk(World, &GroundBuffer->P, &ChunkCenterP))
                    {
                        FurthestBuffer = 0;
                        break;
                    }
                    else if(IsValid(GroundBuffer->P))
                    {
                        RelP = Subtract(World, &GroundBuffer->P, &GameState->CameraP);
                        real32 BufferLengthSq = LengthSq(RelP);
                        if(FurthestBufferLengthSq < BufferLengthSq)
                        {
                            FurthestBufferLengthSq = BufferLengthSq;
                            FurthestBuffer = GroundBuffer;
                        }
                    }
                    else
                    {
                        FurthestBufferLengthSq = Real32Maximum;
                        FurthestBuffer = GroundBuffer;
                    }
                }

                if(FurthestBuffer)
                {
                    FillGroundChunk(TranState, GameState, FurthestBuffer, &ChunkCenterP, TileSideInMeters);
                }
            }
        }
    }

    // NOTE(casey): Ground chunk rendering
    for(uint32 GroundBufferIndex = 0;
        GroundBufferIndex < TranState->GroundBufferCount;
        ++GroundBufferIndex)
    {
        ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
        if(IsValid(GroundBuffer->P))
        {
            loaded_bitmap *Bitmap = &GroundBuffer->Bitmap;
            v2 Delta = Subtract(GameState->World, &GroundBuffer->P, &GameState->CameraP);        

            real32 GroundSideInMeters = World->ChunkDimInMeters.x;
            Transform.SortBias = -1000.0f;
//            PushBitmap(RenderGroup, Transform, Bitmap, GroundSideInMeters, V3(Delta, 0));

            PushRectOutline(RenderGroup, Transform, V3(Delta, 0), V2(GroundSideInMeters, GroundSideInMeters),
                            V4(1.0f, 1.0f, 0.0f, 1.0f), 0.015f);
        }
    }

    Transform.SortBias = 0.0f;

    // NOTE(paul): Render decorations
    for(u32 DecorationIndex = 0;
        DecorationIndex < GameState->WorldTileCount;
        ++DecorationIndex)
    {
        decoration *Decoration = GameState->Decorations + DecorationIndex;
        if(IsValid(Decoration->BitmapID))
        {
            v2 Delta = Subtract(GameState->World, &Decoration->P, &GameState->CameraP) - V2(2.0f, 2.0f);
            if(IsInRectangle(SimBounds, Delta))
            {
                PushBitmap(RenderGroup, Transform, Decoration->BitmapID, Decoration->Height, V3(Delta, 0));
            }
        }
    }
    
    if(GameState->EditMode == EditMode_Terrain)
    {
        TerrainEditMode(RenderGroup, &TextRenderGroup, GameState, TranState, Input, &MouseChunkP, TileSideInMeters);
    }
    else if(GameState->EditMode == EditMode_Decoration)
    {
        DecorationEditMode(RenderGroup, &TextRenderGroup, GameState, TranState, Input, &MouseChunkP,
                           TileSideInMeters, PixelsToMeters);
    }
    
    PushRectOutline(RenderGroup, Transform, V3(0, 0, 0), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1));
    PushRectOutline(RenderGroup, Transform, V3(0, 0, 0), GetDim(SimBounds), V4(1.0f, 0.0f, 1.0f, 1));
    PushRect(RenderGroup, Transform, V3(0, 0, 0), 0.2f*V2(TileSideInMeters, TileSideInMeters), V4(1, 0, 0, 1));

    EndRenderGroup(RenderGroup);
    EndRenderGroup(&TextRenderGroup);
    EndTemporaryMemory(RenderMemory);
    
    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);
}


