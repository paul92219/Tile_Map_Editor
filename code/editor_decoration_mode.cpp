/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Handy Paul $
   $Notice: (C) Copyright 2023 by Handy Paul, Inc. All Rights Reserved. $
   ======================================================================== */

global_variable assetset_id AssetsetID;
global_variable u32 SpriteIndex;

internal void
WriteDecorations(char *FileName, u32 Count, decoration *Decorations)
{
    uint32 ContentSize = sizeof(decoration)*Count;
    Platform.DEBUGWriteEntireFile(FileName, ContentSize, Decorations);
}

internal void
ShowAssetMenuBar(render_group *RenderGroup, array_cursor *Cursor, r32 TileDimInMeters)
{
    object_transform Transform = DefaultUprightTransform();
    loaded_assetset *Assetset = PushAssetset(RenderGroup, AssetsetID);
    u32 AssetCountInBar = Cursor->ArrayCount;

    r32 MenuBarWidth = 3.0f;
    r32 HalfWidth = 0.5f*MenuBarWidth;

    r32 CursorWidth = MenuBarWidth;
    r32 CursorHeight = 0.3f;
    r32 CursorHalfHeight = 0.5f*CursorHeight;

    r32 MenuBarHeight = Cursor->ArrayCount*CursorHeight;
    r32 HalfHeight = 0.5f*MenuBarHeight;
    
    r32 OffsetY = 5.5f - HalfHeight;
    r32 OffsetX = -15.5f + HalfWidth;
    
    PushRectOutline(RenderGroup, Transform, V3(OffsetX, OffsetY, 0), V2(MenuBarWidth, MenuBarHeight), V4(0, 1, 0, 1), 0.05f);

    ssa_assetset *Info = GetAssetsetInfo(RenderGroup->Assets, AssetsetID);
    if(Assetset)
    {
        r32 TileOffsetX = OffsetY - HalfWidth;
        r32 TileOffsetY = -HalfWidth;
        for(u32 Index = 0;
            Index < AssetCountInBar;
            ++Index)
        {
            u32 AssetIndex = Cursor->Array[Index];
        }
    }

    r32 CursorOffsetY = (HalfHeight + OffsetY - CursorHalfHeight) - Cursor->ArrayPosition*CursorHeight;
    PushRectOutline(RenderGroup, Transform, V3(OffsetX, CursorOffsetY, 0), V2(CursorWidth, CursorHeight), V4(1, 1, 1, 1), 0.03f);
}

internal void
ShowAssetsetStats(render_group *RenderGroup, game_state *GameState)
{
    DEBUGTextLine(RenderGroup, "Assetset Stats:");
    char TextBuffer[256];
    _snprintf_s(TextBuffer, sizeof(TextBuffer), "Biome: %s", Biomes[GameState->AssetSetStats.Biome]);
    DEBUGTextLine(RenderGroup, TextBuffer);

    _snprintf_s(TextBuffer, sizeof(TextBuffer), "AssetType: %s", AssetTypes[GameState->AssetSetStats.Type]);
    DEBUGTextLine(RenderGroup, TextBuffer);
}

internal void
ReloadAssetset(game_assets *Assets, game_state *GameState)
{
    asset_vector WeightVector = {};
    WeightVector.E[Tag_BiomeType] = 1.0f;
    WeightVector.E[Tag_AssetType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_BiomeType] = (r32)GameState->AssetSetStats.Biome;
    MatchVector.E[Tag_AssetType] = (r32)GameState->AssetSetStats.Type;
    
    assetset_id ID = GetBestMatchAssetsetFrom(Assets, Asset_AssetSet, &MatchVector, &WeightVector);
    if(ID.Value != AssetsetID.Value)
    {
        AssetsetID = ID;
        ssa_assetset *Info = GetAssetsetInfo(Assets, AssetsetID);
        if((Info->AssetCount > 10))
        {
            ReInitializeCursor(&GameState->AssetMenuBarCursor, 10);
        }
        else
        {
            ReInitializeCursor(&GameState->AssetMenuBarCursor, Info->AssetCount);
        }
    }
}

internal void
AddDecoration(render_group *RenderGroup, game_state *GameState, world_position *MouseP, r32 PixelsToMeters)
{
    u32 DecorationIndex = GetTileIndexFromChunkPosition(GameState, MouseP);
    if((MouseP->ChunkX >= 0) && (MouseP->ChunkY >= 0))
    {
        loaded_assetset *Assetset = PushAssetset(RenderGroup, AssetsetID);
        ssa_assetset *AssetsetInfo = GetAssetsetInfo(RenderGroup->Assets, AssetsetID);
        if(Assetset)
        {
            decoration *Decoration = GameState->Decorations + DecorationIndex;
            animated_decoration *AnimatedDecoration = GameState->AnimatedDecorations + DecorationIndex;
            Decoration->DecorationIndex = DecorationIndex;
            if(AssetsetInfo->DataType == Asset_SpriteSheet)
            {
                u32 ArrayIndex = GameState->AssetMenuBarCursor.Array[GameState->AssetMenuBarCursor.ArrayPosition];
                spritesheet_id ID = {};
                ID.Value = GetAssetIDFromAssetset(RenderGroup->Assets, AssetsetInfo, Assetset, ArrayIndex);

                ssa_spritesheet *SpriteSheetInfo = GetSpriteSheetInfo(RenderGroup->Assets, ID);
                loaded_spritesheet *SpriteSheet = PushSpriteSheet(RenderGroup, ID, RenderGroup->GenerationID);

                bitmap_id BitmapID = SpriteSheet->SpriteIDs[0];
                BitmapID.Value += SpriteSheet->BitmapIDOffset;
                ssa_bitmap *BitmapInfo = GetBitmapInfo(RenderGroup->Assets, BitmapID);
                v2 BitmapDimInMeters = PixelsToMeters*V2i(BitmapInfo->Dim[0], BitmapInfo->Dim[1]);

                asset_tag_result Tags = GetAssetTags(RenderGroup->Assets, ID.Value);
                Decoration->AssetTypeID = AssetsetInfo->DataType;
                Decoration->IsSpriteSheet = true;

                for(u32 TagIndex = 0;
                    TagIndex < Tag_Count;
                    ++TagIndex)
                {
                    if(Tags.WeightVector.E[TagIndex] != 0.0f)
                    {
                        Decoration->Tags[Decoration->TagCount].ID = TagIndex;
                        Decoration->Tags[Decoration->TagCount].Value = Tags.MatchVector.E[TagIndex];
                        ++Decoration->TagCount;
                    }
                }
            
                Decoration->SpriteSheetID = ID;
                Decoration->Height = BitmapDimInMeters.y;

                tile_position TileP = TilePositionFromChunkPosition(MouseP);
                Decoration->P = ChunkPositionFromTilePosition(GameState->World, TileP.TileX, TileP.TileY);

                // NOTE(paul): Only for drawing
                AnimatedDecoration->SpriteIndex = 0;
                AnimatedDecoration->Info = SpriteSheetInfo;
                AnimatedDecoration->SpriteSheet = SpriteSheet;
            }
            else
            {
                u32 AssetIndex = GameState->AssetMenuBarCursor.Array[GameState->AssetMenuBarCursor.ArrayPosition];

                bitmap_id ID = {}; 
                ID.Value = Assetset->AssetIDs[AssetIndex] + Assetset->IDOffset;

                ssa_bitmap *BitmapInfo = GetBitmapInfo(RenderGroup->Assets, ID);
                v2 BitmapDimInMeters = PixelsToMeters*V2i(BitmapInfo->Dim[0], BitmapInfo->Dim[1]);

                asset_tag_result Tags = GetAssetTags(RenderGroup->Assets, ID.Value);
                Decoration->AssetTypeID = AssetsetInfo->DataType;
                Decoration->IsSpriteSheet = false;

                for(u32 TagIndex = 0;
                    TagIndex < Tag_Count;
                    ++TagIndex)
                {
                    if(Tags.WeightVector.E[TagIndex] != 0.0f)
                    {
                        Decoration->Tags[Decoration->TagCount].ID = TagIndex;
                        Decoration->Tags[Decoration->TagCount].Value = Tags.MatchVector.E[TagIndex];
                        ++Decoration->TagCount;
                    }
                }
            
                Decoration->BitmapID = ID;
                Decoration->Height = BitmapDimInMeters.y;

                tile_position TileP = TilePositionFromChunkPosition(MouseP);
                Decoration->P = ChunkPositionFromTilePosition(GameState->World, TileP.TileX, TileP.TileY);
            }
        }
    }
}

internal void
RemoveDecoration(render_group *RenderGroup, game_state *GameState, world_position *MouseP, r32 PixelsToMeters)
{
    u32 DecorationIndex = GetTileIndexFromChunkPosition(GameState, MouseP);
    if((MouseP->ChunkX >= 0) && (MouseP->ChunkY >= 0))
    {
        GameState->Decorations[DecorationIndex].IsSpriteSheet = 0;
        GameState->Decorations[DecorationIndex].BitmapID.Value = 0;
        GameState->Decorations[DecorationIndex].Height = 0;
        GameState->Decorations[DecorationIndex].P = {};
    }
}

internal void
DecorationEditMode(render_group *RenderGroup, render_group *TextRenderGroup, game_state *GameState,
                   transient_state *TranState, game_input *Input, world_position *MouseChunkP,
                   r32 TileSideInMeters, r32 PixelsToMeters, v2 MouseCameraRelP)
{
    // TODO(paul): Make this work with spritesheets

    object_transform Transform = DefaultUprightTransform();
    if(Input->MouseButtons[0].EndedDown)
    {
        AddDecoration(RenderGroup, GameState, MouseChunkP, PixelsToMeters);
        WriteDecorations("decorations.bin", GameState->WorldTileCount, GameState->Decorations);
    }

    if(Input->MouseButtons[2].EndedDown)
    {
        RemoveDecoration(RenderGroup, GameState, MouseChunkP, PixelsToMeters);
        WriteDecorations("decorations.bin", GameState->WorldTileCount, GameState->Decorations);
    }

    ssa_assetset *Info = GetAssetsetInfo(TranState->Assets, AssetsetID);
    ChangeCursorPositionFor(&GameState->AssetMenuBarCursor, Info->AssetCount, Input->MouseZ);
    ShowTest(TextRenderGroup, &GameState->AssetMenuBarCursor);
    ShowAssetMenuBar(RenderGroup, &GameState->AssetMenuBarCursor, TileSideInMeters);
    ShowAssetsetStats(TextRenderGroup, GameState);


    ReloadAssetset(TranState->Assets, GameState);

    v2 D = MouseCameraRelP;

    loaded_assetset *Assetset = PushAssetset(RenderGroup, AssetsetID, true);
    ssa_assetset *AssetsetInfo = GetAssetsetInfo(TranState->Assets, AssetsetID);
    if(Assetset && AssetsetInfo)
    {
        if(AssetsetInfo->DataType == Asset_SpriteSheet)
        {
            u32 ArrayIndex = GameState->AssetMenuBarCursor.Array[GameState->AssetMenuBarCursor.ArrayPosition];
            spritesheet_id ID = {};
            ID.Value = GetAssetIDFromAssetset(TranState->Assets, AssetsetInfo, Assetset, ArrayIndex);
            ssa_spritesheet *SpriteSheetInfo = GetSpriteSheetInfo(TranState->Assets, ID);
            loaded_spritesheet *SpriteSheet = PushSpriteSheet(RenderGroup, ID, RenderGroup->GenerationID);

            SpriteIndex = GetSpriteIndex(GameState->Time, SpriteSheetInfo->SpriteCount);

            bitmap_id BitmapID = SpriteSheet->SpriteIDs[SpriteIndex];
            BitmapID.Value += SpriteSheet->BitmapIDOffset;
            ssa_bitmap *BitmapInfo = GetBitmapInfo(TranState->Assets, BitmapID);
            v2 BitmapDimInMeters = PixelsToMeters*V2i(BitmapInfo->Dim[0], BitmapInfo->Dim[1]);

            PushBitmap(RenderGroup, Transform, BitmapID, BitmapDimInMeters.y, V3(-D, 0) - V3(0.5f, 0.5f, 0));
        }
        else
        {
            u32 ArrayIndex = GameState->AssetMenuBarCursor.Array[GameState->AssetMenuBarCursor.ArrayPosition];
            bitmap_id BitmapID = GetBitmapFromAssetset(TranState->Assets, AssetsetInfo, Assetset,
                                                       ArrayIndex);

            ssa_bitmap *BitmapInfo = GetBitmapInfo(TranState->Assets, BitmapID);
            v2 BitmapDimInMeters = PixelsToMeters*V2i(BitmapInfo->Dim[0], BitmapInfo->Dim[1]);

            PushBitmap(RenderGroup, Transform, BitmapID, BitmapDimInMeters.y, V3(-D, 0) - V3(0.5f, 0.5f, 0));
        }
    }
}



