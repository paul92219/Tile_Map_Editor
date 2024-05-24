#if !defined(EDITOR_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Paul Solodrai $
   ======================================================================== */

#include "editor_platform.h"
#include "editor_shared.h"

#define DLIST_INSERT(Sentinel, Element)         \
    (Element)->Next = (Sentinel)->Next;         \
    (Element)->Prev = (Sentinel);               \
    (Element)->Next->Prev = (Element);          \
    (Element)->Prev->Next = (Element); 
#define DLIST_INSERT_AS_LAST(Sentinel, Element)         \
    (Element)->Next = (Sentinel);               \
    (Element)->Prev = (Sentinel)->Prev;         \
    (Element)->Next->Prev = (Element);          \
    (Element)->Prev->Next = (Element); 

#define DLIST_INIT(Sentinel) \
    (Sentinel)->Next = (Sentinel); \
    (Sentinel)->Prev = (Sentinel);

#define FREELIST_ALLOCATE(Result, FreeListPointer, AllocationCode)             \
    (Result) = (FreeListPointer); \
    if(Result) {FreeListPointer = (Result)->NextFree;} else {Result = AllocationCode;}
#define FREELIST_DEALLOCATE(Pointer, FreeListPointer) \
    if(Pointer) {(Pointer)->NextFree = (FreeListPointer); (FreeListPointer) = (Pointer);}

struct memory_arena
{
    memory_index Size;
    uint8 *Base;
    memory_index Used;

    int32 TempCount;
};

struct temporary_memory
{
    memory_arena *Arena;
    memory_index Used;
};

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

//
//
//

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Pointer) ZeroSize(Count*sizeof((Pointer)[0]), Pointer)
inline void
ZeroSize(memory_index Size, void *Ptr)
{
    // TODO(casey): Check this guy for performance
    uint8 *Byte = (uint8 *)Ptr;
    while(Size--)
    {
        *Byte++ = 0;
    }
}

inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (uint8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

inline memory_index
GetAlignmentOffset(memory_arena *Arena, memory_index Alignment)
{
    memory_index AlignmentOffset = 0;
    
    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    memory_index AlignmentMask = Alignment - 1;
    if(ResultPointer & AlignmentMask)
    {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }

    return(AlignmentOffset);
}

enum arena_push_flag
{
    ArenaFlag_ClearToZero = 0x1,
};
struct arena_push_params
{
    u32 Flags;
    u32 Alignment;
};

inline arena_push_params
DefaultArenaParams(void)
{
    arena_push_params Params;
    Params.Flags = ArenaFlag_ClearToZero;
    Params.Alignment = 4;
    return(Params);
}

inline arena_push_params
AlignNoClear(u32 Alignment)
{
    arena_push_params Params = DefaultArenaParams();
    Params.Flags &= ~ArenaFlag_ClearToZero;
    Params.Alignment = Alignment;
    return(Params);
}

inline arena_push_params
Align(u32 Alignment, b32 Clear)
{
    arena_push_params Params = DefaultArenaParams();
    if(Clear)
    {
        Params.Flags |= ArenaFlag_ClearToZero;
    }
    else
    {
        Params.Flags &= ~ArenaFlag_ClearToZero;
    }
    Params.Alignment = Alignment;
    return(Params);
}

inline arena_push_params
NoClear(void)
{
    arena_push_params Params = DefaultArenaParams();
    Params.Flags &= ~ArenaFlag_ClearToZero;
    return(Params);
}

inline memory_index
GetArenaSizeRemaining(memory_arena *Arena, arena_push_params Params = DefaultArenaParams())
{
    memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Params.Alignment));

    return(Result);
}

// TODO(casey): Optional "clear" parameter!!!!
#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(Arena, (Count)*sizeof(type), ## __VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ## __VA_ARGS__)
#define PushCopy(Arena, Size, Source, ...) Copy(Size, Source, PushSize_(Arena, Size, ## __VA_ARGS__))
inline memory_index
GetEffectiveSizeFor(memory_arena *Arena, memory_index SizeInit, arena_push_params Params = DefaultArenaParams())
{
    memory_index Size = SizeInit;
        
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Params.Alignment);
    Size += AlignmentOffset;

    return(Size);
}

inline b32
ArenaHasRoomFor(memory_arena *Arena, memory_index SizeInit, arena_push_params Params = DefaultArenaParams())
{
    memory_index Size = GetEffectiveSizeFor(Arena, SizeInit, Params);
    b32 Result = ((Arena->Used + Size) <= Arena->Size);
    return(Result);
}

inline void *
PushSize_(memory_arena *Arena, memory_index SizeInit, arena_push_params Params = DefaultArenaParams())
{
    memory_index Size = GetEffectiveSizeFor(Arena, SizeInit, Params);
    
    Assert((Arena->Used + Size) <= Arena->Size);

    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Params.Alignment);
    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;

    Assert(Size >= SizeInit);

    if(Params.Flags & ArenaFlag_ClearToZero)
    {
        ZeroSize(SizeInit, Result);
    }
    
    return(Result);
}

// NOTE(casey): This is generally not for production use, this is probably
// only really something we need during testing, but who knows
inline char *
PushString(memory_arena *Arena, char *Source)
{
    u32 Size = 1;
    for(char *At = Source;
        *At;
        ++At)
    {
        ++Size;
    }
    
    char *Dest = (char *)PushSize_(Arena, Size, NoClear());
    for(u32 CharIndex = 0;
        CharIndex < Size;
        ++CharIndex)
    {
        Dest[CharIndex] = Source[CharIndex];
    }

    return(Dest);
}
inline wchar_t *
PushStringW(memory_arena *Arena, wchar_t *Source)
{
    u32 Size = StringSizeW(Source);
    if(Size == 0)
    {
        Size = 1;
    }
    
    wchar_t *Dest = (wchar_t *)PushSize_(Arena, Size + 1, NoClear());
    for(u32 CharIndex = 0;
        CharIndex < Size;
        ++CharIndex)
    {
        Dest[CharIndex] = Source[CharIndex];
    }

    Dest[Size] = 0;
    
    return(Dest);
}

inline char *
PushAndNullTerminate(memory_arena *Arena, u32 Length, char *Source)
{
    char *Dest = (char *)PushSize_(Arena, Length + 1, NoClear());
    for(u32 CharIndex = 0;
        CharIndex < Length;
        ++CharIndex)
    {
        Dest[CharIndex] = Source[CharIndex];
    }
    Dest[Length] = 0;

    return(Dest);
}

inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;

    Result.Arena = Arena;
    Result.Used = Arena->Used;

    ++Arena->TempCount;

    return(Result);
}

inline void
EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

inline void
Clear(memory_arena *Arena)
{
    InitializeArena(Arena, Arena->Size, Arena->Base);
}

inline void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

inline void
SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size, arena_push_params Params = DefaultArenaParams())
{
    Result->Size = Size;
    Result->Base = (uint8 *)PushSize_(Arena, Size, Params);
    Result->Used = 0;
    Result->TempCount = 0;
}

inline void *
Copy(memory_index Size, void *SourceInit, void *DestInit)
{
    u8 *Source = (u8 *)SourceInit;
    u8 *Dest = (u8 *)DestInit;
    while(Size--) {*Dest++ = *Source++;}

    return(DestInit);
}

//#include "d:/paul/Spellweaver_Saga_game/AssetFileBuilder/code/file_formats.h"
#include "editor_file_formats.h"
#include "editor_render_group.h"
#include "editor_random.h"
#include "editor_asset.h"
#include "editor_ui.h"
#include "editor_world.h"
#include "editor_terrain_mode.h"
#include "editor_decoration_mode.h"
#include "editor_collision_mode.h"
#include "editor_assets_mode.h"

enum edit_mode
{
    EditMode_None,
    EditMode_Terrain,
    EditMode_Decoration,
    EditMode_Collision,
    EditMode_Assets,
    EditMode_Count,
};

static const char *EditModeText[] =
{
    "None",
    "Terrain",
    "Decoration",
    "Collision",
    "Assets",
};

struct editor_state
{
    bool32 IsInitialized;
    memory_arena ModeArena;

    world *World;
//    ui_state *UI;
    
    edit_mode EditMode;
    union
    {
        edit_mode_asset *AssetMode;
        edit_mode_terrain *TerrainMode;
        edit_mode_decoration *DecorationMode;
        edit_mode_collision *CollisionMode;
    };
};

struct task_with_memory
{
    bool32 BeingUsed;
    b32 DependsOnEditMode;
    memory_arena Arena;

    temporary_memory MemoryFlush;
};

struct ground_buffer
{
    // NOTE(casey): An invalid P tells us that this ground_buffer has not been filled 
    world_position P; // NOTE(casey): This is the center of the bitmap
    loaded_bitmap Bitmap;
};

struct transient_state
{
    bool32 IsInitialized;
    memory_arena TranArena;
    memory_arena StringArena;

    u32 MainGenerationID;
    
    task_with_memory Tasks[4];

    game_assets *Assets;

    uint32 GroundBufferCount;
    ground_buffer *GroundBuffers;

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;
};

inline u32
GetSpriteIndex(r32 Time, u32 SpriteCount, u32 Speed = 0)
{
    u32 Result = 0;
    if(Speed)
    {
        Result = FloorReal32ToInt32(Time*Speed) % SpriteCount;
    }
    else
    {
        Result = FloorReal32ToInt32(Time*SpriteCount) % SpriteCount;
    }
    
    return(Result);
}

global_variable platform_api Platform;

internal task_with_memory *BeginTaskWithMemory(transient_state *TranState);
internal void EndTaskWithMemory(task_with_memory *Task);

#define EDITOR_H
#endif
