#include "RSDK/Core/RetroEngine.hpp"

using namespace RSDK;

#if RETRO_REV0U
#include "Legacy/UserStorageLegacy.cpp"
#endif

// Macro to access the header variables of a block of memory.
// Note that this is pointless if the pointer is already pointing directly at the header rather than the memory after it.
#define HEADER(memory, header_value) memory[-HEADER_SIZE + header_value]

// Every block of allocated memory is prefixed with a header that consists of the following four longwords.
enum {
    // Whether the block of memory is actually allocated or not.
    HEADER_ACTIVE,
    // Which 'data set' this block of memory belongs to.
    HEADER_SET_ID,
    // The offset in the buffer which the block of memory begins at.
    HEADER_DATA_OFFSET,
    // How long the block of memory is (measured in 'uint32's).
    HEADER_DATA_LENGTH,
    // This is not part of the header: it's just a bit of enum magic to calculate the size of the header.
    HEADER_SIZE
};

DataStorage RSDK::dataStorage[DATASET_MAX];

#if RETRO_PLATFORM == RETRO_KALLISTIOS

struct StorageFlags
{
    using underlying = uint32;

    static constexpr underlying setIDShift = 2u;

    enum E : underlying
    {
        none = 0,
        used = 1u << 0u,
        pinned = 1u << 1u,
        setIDMask = 7u << setIDShift
    };
};

struct StorageHeader
{
    StorageFlags::underlying flags;
    uint32 poolOffset; // original offset (in bytes) in the owning pool - used for defragmenting
    uint32 capacity; // offset from end of this header to the next in 32-bit ints
    uint32 length; // length in 32-bit integers

    [[nodiscard]] __always_inline bool IsUsed() const {
        return (flags & StorageFlags::used) != 0;
    }

    [[nodiscard]] __always_inline bool IsPinned() const {
        return (flags & StorageFlags::pinned) != 0;
    }

    // only valid if IsUsed() is also true
    [[nodiscard]] __always_inline bool HasExcess() const {
        return length < capacity;
    }

    [[nodiscard]] __always_inline uint32 GetDataSet() const {
        return (flags & StorageFlags::setIDMask) >> StorageFlags::setIDShift;
    }

    [[nodiscard]] __always_inline StorageHeader* VeryUnsafeNext() {
        return reinterpret_cast<StorageHeader*>(reinterpret_cast<uint32*>(this + 1) + capacity);
    }

    [[nodiscard]] __always_inline uint32* GetDataPtr() {
        return reinterpret_cast<uint32*>(this + 1);
    }

    [[nodiscard]] __always_inline static constexpr uint32 SizeInts() {
        return static_cast<uint32>(sizeof(StorageHeader) / sizeof(uint32));
    }
};

#endif

bool32 RSDK::InitStorage()
{
    // Storage limits.
#if RETRO_PLATFORM == RETRO_KALLISTIOS  /* SAYGA DREAMCAST: Where Sonic Belongs! */
                                                                       // RAM: 32  / 16  MB
                                                                       // -----------------
    dataStorage[DATASET_STG].storageLimit = 700000 + (DBL_MEM? 4 : 4) * 1024 * 1024; // 4.7   / 4.7   MB
    dataStorage[DATASET_STR].storageLimit = (DBL_MEM? 3 : 3) * 32 * 1024; // 96  / 96  KB
    dataStorage[DATASET_TMP].storageLimit = 150000 + (DBL_MEM? 6 : 3) * 1024 * 1024; // 6.15   / 3.15   MB
                                                                     //   -----------------
#else                                                                // Total: 16+ / 7+  MB 
                                        /* PCs AND BORING SHIT */
    dataStorage[DATASET_STG].storageLimit = 24 * 1024 * 1024; // 24 MB
    dataStorage[DATASET_MUS].storageLimit =  8 * 1024 * 1024; //  8 MB
    dataStorage[DATASET_SFX].storageLimit = 32 * 1024 * 1024; // 32 MB
    dataStorage[DATASET_STR].storageLimit =  2 * 1024 * 1024; //  2 MB
    dataStorage[DATASET_TMP].storageLimit =  8 * 1024 * 1024; //  8 MB
                                                       // Total: 74 MB LMAO, YEAH RIGHT.
#endif

    for (int32 s = 0; s < DATASET_MAX; ++s) {
        dataStorage[s].usedStorage = 0;
        dataStorage[s].entryCount  = 0;
        dataStorage[s].clearCount  = 0;

        // DCWIP
        if (dataStorage[s].storageLimit == 0) {
            dataStorage[s].memoryTable = NULL;
            continue;
        }

        dataStorage[s].memoryTable = (uint32 *)malloc(dataStorage[s].storageLimit);

        if (dataStorage[s].memoryTable == NULL)
            return false;

#if RETRO_PLATFORM == RETRO_KALLISTIOS
        auto* header = reinterpret_cast<StorageHeader*>(dataStorage[s].memoryTable);
        *header = {};
        header->length = 0;
        header->capacity = (dataStorage[s].storageLimit - sizeof(StorageHeader)) / sizeof(uint32);
        const void* poolEnd = reinterpret_cast <const uint8 *>(dataStorage[s].memoryTable) + dataStorage[s].storageLimit;
        assert(header->VeryUnsafeNext() == poolEnd);
#endif
    }

    return true;
}

void RSDK::ReleaseStorage()
{
    for (int32 s = 0; s < DATASET_MAX; ++s) {
        if (dataStorage[s].memoryTable != NULL)
            free(dataStorage[s].memoryTable);

        dataStorage[s].usedStorage = 0;
        dataStorage[s].entryCount  = 0;
        dataStorage[s].clearCount  = 0;
    }

    // this code isn't in steam executable, since it omits the "load datapack into memory" feature.
    // I don't think it's in the console versions either, but this never seems to be freed in those versions.
    // so, I figured doing it here would be the neatest.
#if !RETRO_USE_ORIGINAL_CODE
    for (int32 p = 0; p < dataPackCount; ++p) {
        if (dataPacks[p].fileBuffer)
            free(dataPacks[p].fileBuffer);

        dataPacks[p].fileBuffer = NULL;
    }
#endif
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS
namespace {
const char* DataSetToString(uint32 dataSet) {
    switch (dataSet) {
        case DATASET_STG:
            return "DATASET_STG";
        case DATASET_STR:
            return "DATASET_STR";
        case DATASET_TMP:
            return "DATASET_TMP";
        case DATASET_MAX:
            return "DATASET_MAX?!";
        default:
            return "> DATASET_MAX?!?!";
    }
}

void PrintAllocState(uint32 dataSet, const DataStorage* storage, const void* var, uint32 inputSize, const char* file, size_t line) {
    printf("[%s] ", var == nullptr ? "NG" : "OK");

    const char* dataSetName = DataSetToString(dataSet);

    if (storage) {
        printf("%s %lu (%lu free) (from %s:%u)\n", dataSetName, inputSize,
               storage->storageLimit - (sizeof(int32) * storage->usedStorage), file, line);
    } else {
        printf("%s\n", dataSetName);
    }
}

uint32 DataSetFromPointer(const void* ptr) {
    // assuming non-null

    for (uint32 i = 0; i < DATASET_MAX; ++i) {
        const auto& storage = dataStorage[i];

        if (storage.memoryTable == nullptr || !storage.storageLimit) {
            continue;
        }

        const uint8* poolBegin = reinterpret_cast<const uint8*>(storage.memoryTable);
        const uint8* poolEnd = poolBegin + storage.storageLimit;

        if (ptr >= poolBegin && ptr < poolEnd) {
            return i;
        }
    }

    return DATASET_MAX;
}
}

void RSDK::PinStorage_(void** pVar, const char* file, size_t line) {
    if (pVar == nullptr || *pVar == nullptr) {
        return;
    }

    if (DataSetFromPointer(*pVar) == DATASET_MAX) {
        return;
    }

    auto* header = static_cast<StorageHeader*>(*pVar) - 1;
    header->flags |= StorageFlags::pinned;
}

void RSDK::UnPinStorage_(void** pVar, const char* file, size_t line) {
    if (pVar == nullptr || *pVar == nullptr) {
        return;
    }

    if (DataSetFromPointer(*pVar) == DATASET_MAX) {
        return;
    }

    auto* header = static_cast<StorageHeader*>(*pVar) - 1;
    header->flags &= ~StorageFlags::pinned;
}
#endif

void RSDK::AllocateStorage_(void **dataPtr, uint32 size, StorageDataSets dataSet, bool32 clear, const char* file, size_t line)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if (dataPtr == nullptr) {
        return;
    }

    *dataPtr = nullptr;

    if (static_cast<uint32>(dataSet) >= DATASET_MAX) {
        return;
    }

    DataStorage* storage = &dataStorage[dataSet];

    if (storage->memoryTable == nullptr) {
        return; // :(
    }

    if (storage->usedStorage == 0) {
        auto* header = reinterpret_cast<StorageHeader*>(storage->memoryTable);
        *header = {};
        header->capacity = (storage->storageLimit - sizeof(StorageHeader)) / sizeof(uint32);
        const void* poolEnd = reinterpret_cast<const uint8*>(storage->memoryTable) + storage->storageLimit;
        assert(header->VeryUnsafeNext() > header);
        assert(header->VeryUnsafeNext() == poolEnd);
    }

    static_assert(sizeof(StorageHeader) % sizeof(uint32) == 0, "nope");
    const uint32 size_i = AlignUp(size, sizeof(uint32)) / sizeof(uint32);
    bool ranGC = false;

    if (storage->entryCount >= STORAGE_ENTRY_COUNT ||
        sizeof(uint32) * (storage->usedStorage + size_i + StorageHeader::SizeInts()) >= storage->storageLimit) {
        DefragmentAndGarbageCollectStorage(dataSet);
        ranGC = true;

        if (storage->entryCount >= STORAGE_ENTRY_COUNT ||
            sizeof(uint32) * (storage->usedStorage + size_i + StorageHeader::SizeInts()) >= storage->storageLimit) {
            PrintAllocState(dataSet, storage, *dataPtr, size, file, line);
            return; // :(
        }
    }

    StorageHeader* fitHeader = nullptr;

    while (true)
    {
        StorageHeader* prevHeader = nullptr;
        StorageHeader* currHeader = reinterpret_cast<StorageHeader*>(storage->memoryTable);
        const void* poolEnd = reinterpret_cast<const uint8*>(storage->memoryTable) + storage->storageLimit;

        // DCTODO: account for pinning logic! push pinned objects as far to the end of memoryTable as possible
        while (currHeader < poolEnd) {
            if (!currHeader->IsUsed()) {
                // while we're here, defragment free space
                if (prevHeader != nullptr && !prevHeader->IsUsed()) {
                    prevHeader->capacity += StorageHeader::SizeInts() + currHeader->capacity;
                    currHeader = prevHeader;
                }

                if (currHeader->capacity >= size_i) {
                    fitHeader = currHeader;
                    break;
                }
            }

            prevHeader = currHeader;
            currHeader = currHeader->VeryUnsafeNext();
        }

        if (fitHeader != nullptr || ranGC) {
            break;
        }

        printf("%s failed to find big enough block for alloc size %u - running GC and retrying\n",
               DataSetToString(dataSet),
               size);

        // would be nice if this function just reported whether or not anything got done...
        DefragmentAndGarbageCollectStorage(dataSet);
        ranGC = true;
    }

    if (fitHeader == nullptr) {
        PrintAllocState(dataSet, storage, *dataPtr, size, file, line);
        return; // :(
    }

    auto* data = fitHeader->GetDataPtr();

    fitHeader->flags = StorageFlags::used | (static_cast<uint32>(dataSet) << StorageFlags::setIDShift);
    fitHeader->poolOffset = static_cast<uint32>(reinterpret_cast<uintptr_t>(data) - reinterpret_cast<uintptr_t>(storage->memoryTable));

    fitHeader->length = size_i;

    // if there's enough space to partition this block, then do it.
    // otherwise, if there's any excess space, it'll be reclaimed later.
    const uint32 blockRemainder = fitHeader->capacity - size_i;
    if (blockRemainder > StorageHeader::SizeInts()) {
        fitHeader->capacity = size_i;

        auto* partition = fitHeader->VeryUnsafeNext();
        *partition = {};
        partition->capacity = blockRemainder - StorageHeader::SizeInts();
    }

    if (clear) {
        memset(data, 0, static_cast<size_t>(sizeof(uint32)) * static_cast<size_t>(fitHeader->length));
    }

    uint32** varPtr = reinterpret_cast<uint32**>(dataPtr);
    *varPtr = data;

    storage->dataEntries[storage->entryCount] = varPtr;
    storage->storageEntries[storage->entryCount] = data;
    ++storage->entryCount;
    storage->usedStorage += StorageHeader::SizeInts() + size_i;
    //PrintAllocState(dataSet, storage, *dataPtr, inputSize, file, line);
#else
    uint32 inputSize = size;
    uint32 **data = (uint32 **)dataPtr;
    *data         = NULL;

    if ((uint32)dataSet < DATASET_MAX) {
        // Align allocation to prevent unaligned memory accesses later on.
        const uint32 size_aligned = size & -(int32)sizeof(void *);

        if (size_aligned < size)
            size = size_aligned + sizeof(void *);

        DataStorage *storage = &dataStorage[dataSet];

        if (dataStorage[dataSet].entryCount < STORAGE_ENTRY_COUNT) {
#if !RETRO_USE_ORIGINAL_CODE
            // Bug: The original release never takes into account the size of the header when checking if there's enough storage left.
            // Omitting this will overflow the memory pool when (storageLimit - usedStorage + size) < header size (16 bytes here).
            if (storage->usedStorage * sizeof(uint32) + size + (HEADER_SIZE * sizeof(uint32)) < storage->storageLimit) {
#else
            if (storage->usedStorage * sizeof(uint32) + size < storage->storageLimit) {
#endif
                // HEADER_ACTIVE
                storage->memoryTable[storage->usedStorage] = true;
                ++storage->usedStorage;

                // HEADER_SET_ID
                storage->memoryTable[storage->usedStorage] = dataSet;
                ++storage->usedStorage;

                // HEADER_DATA_OFFSET
                storage->memoryTable[storage->usedStorage] = storage->usedStorage + HEADER_SIZE - HEADER_DATA_OFFSET;
                ++storage->usedStorage;

                // HEADER_DATA_LENGTH
                storage->memoryTable[storage->usedStorage] = size;
                ++storage->usedStorage;

                *data = &storage->memoryTable[storage->usedStorage];
                storage->usedStorage += size / sizeof(uint32);

                dataStorage[dataSet].dataEntries[storage->entryCount]    = data;
                dataStorage[dataSet].storageEntries[storage->entryCount] = *data;

                ++storage->entryCount;
            }
            else {
                // We've run out of room, so perform defragmentation and garbage-collection.
                DefragmentAndGarbageCollectStorage(dataSet);

                // If there is now room, then perform allocation.
                // Yes, this really is a massive chunk of duplicate code.
#if !RETRO_USE_ORIGINAL_CODE
                if (storage->usedStorage * sizeof(uint32) + size + (HEADER_SIZE * sizeof(uint32)) < storage->storageLimit) {
#else
                if (storage->usedStorage * sizeof(uint32) + size < storage->storageLimit) {
#endif
                    // HEADER_ACTIVE
                    storage->memoryTable[storage->usedStorage] = true;
                    ++storage->usedStorage;

                    // HEADER_SET_ID
                    storage->memoryTable[storage->usedStorage] = dataSet;
                    ++storage->usedStorage;

                    // HEADER_DATA_OFFSET
                    storage->memoryTable[storage->usedStorage] = storage->usedStorage + HEADER_SIZE - HEADER_DATA_OFFSET;
                    ++storage->usedStorage;

                    // HEADER_DATA_LENGTH
                    storage->memoryTable[storage->usedStorage] = size;
                    ++storage->usedStorage;

                    *data = &storage->memoryTable[storage->usedStorage];
                    storage->usedStorage += size / sizeof(uint32);

                    dataStorage[dataSet].dataEntries[storage->entryCount]    = data;
                    dataStorage[dataSet].storageEntries[storage->entryCount] = *data;

                    ++storage->entryCount;
                }
            }

            // If there are too many storage entries, then perform garbage collection.
            if (storage->entryCount >= STORAGE_ENTRY_COUNT)
                GarbageCollectStorage(dataSet);

            // Clear the allocated memory if requested.
            if (*data != NULL && clear == (bool32)true)
                memset(*data, 0, size);
        }

        // DCWIP
#if RETRO_PLATFORM == RETRO_KALLISTIOS
        if (*dataPtr == NULL) {
            PrintAllocState(dataSet, storage, *dataPtr, inputSize, file, line);
        }
#endif
    }
#endif
}

void RSDK::RemoveStorageEntry_(void **dataPtr, const char* file, size_t line)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if (dataPtr == nullptr || *dataPtr == nullptr) {
        return;
    }

    if (DataSetFromPointer(*dataPtr) == DATASET_MAX) {
        return;
    }

    StorageHeader* storageHeader = static_cast<StorageHeader*>(*dataPtr) - 1;
    const auto setId = storageHeader->GetDataSet();
    auto* storage = &dataStorage[setId];

    for (uint32 e = 0; e < storage->entryCount; ++e) {
        // this implementation wipes *all* references to the given data...
        if (storage->dataEntries[e] != nullptr && *storage->dataEntries[e] == *dataPtr) {
            *storage->dataEntries[e] = nullptr;
            storage->dataEntries[e] = nullptr;
        }
    }

    uint32 newEntryCount = 0;

    for (uint32 e = 0; e < storage->entryCount; ++e) {
        if (storage->dataEntries[e] == nullptr) {
            continue;
        }

        if (e != newEntryCount) {
            storage->dataEntries[newEntryCount] = storage->dataEntries[e];
            storage->storageEntries[newEntryCount] = storage->storageEntries[e];

            storage->dataEntries[e] = nullptr;
            storage->storageEntries[e] = nullptr;
        }

        ++newEntryCount;
    }

    for (uint32 e = newEntryCount; e < storage->entryCount; ++e) {
        storage->dataEntries[e] = nullptr;
        storage->storageEntries[e] = nullptr;
    }

    storage->entryCount = newEntryCount;
    // wipes used and pinned bits
    storageHeader->flags = static_cast<uint32>(setId) << StorageFlags::setIDShift;
#else
    if (dataPtr != NULL && *dataPtr != NULL) {
        uint32 *data = *(uint32 **)dataPtr;

        uint32 set = HEADER(data, HEADER_SET_ID);
        for (int32 e = 0; e < dataStorage[set].entryCount; ++e) {
#if !RETRO_USE_ORIGINAL_CODE
            // make sure dataEntries[e] isn't null. If it is null by some ungodly chance then it was prolly already freed or something idk
            if (dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e] && *dataPtr == *dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e]) {
                *dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e] = NULL;
                dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e]  = NULL;
            }
#else
            if (*dataPtr == *dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e]) {
                *dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e] = NULL;
                dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e]  = NULL;
            }
#endif

            set = HEADER(data, HEADER_SET_ID);
        }

        uint32 newEntryCount = 0;
        set                  = HEADER(data, HEADER_SET_ID);
        for (uint32 entryID = 0; entryID < dataStorage[set].entryCount; ++entryID) {
            if (dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[entryID]) {
                if (entryID != newEntryCount) {
                    dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[newEntryCount] =
                        dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[entryID];
                    dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[entryID] = NULL;
                    dataStorage[HEADER(data, HEADER_SET_ID)].storageEntries[newEntryCount] =
                        dataStorage[HEADER(data, HEADER_SET_ID)].storageEntries[entryID];
                    dataStorage[HEADER(data, HEADER_SET_ID)].storageEntries[entryID] = NULL;
                }

                ++newEntryCount;
            }

            set = HEADER(data, HEADER_SET_ID);
        }

        dataStorage[HEADER(data, HEADER_SET_ID)].entryCount = newEntryCount;

        for (uint32 e = newEntryCount; e < STORAGE_ENTRY_COUNT; ++e) {
            dataStorage[HEADER(data, HEADER_SET_ID)].dataEntries[e]    = NULL;
            dataStorage[HEADER(data, HEADER_SET_ID)].storageEntries[e] = NULL;
        }

        HEADER(data, HEADER_ACTIVE) = false;
    }
#endif
}

// This defragments the storage, leaving all empty space at the end.
void RSDK::DefragmentAndGarbageCollectStorage(StorageDataSets set)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    DataStorage* storage = &dataStorage[set];

    if (storage->memoryTable == nullptr) {
        return;
    }

    const uint32 clearCount = storage->clearCount++;

    // Perform garbage-collection. This deallocates all memory allocations that are no longer being used.
    GarbageCollectStorage(set);

    // DCTODO: don't run defrag unless a deallocation or unpin has occurred

    bool relocatedStorage = false;
    uint32 activeStorage = 0;

    auto* poolBegin = reinterpret_cast<StorageHeader*>(storage->memoryTable);
    const void* poolEnd = reinterpret_cast<const uint8*>(storage->memoryTable) + storage->storageLimit;

    StorageHeader* lastValidHeader = nullptr;

    // DO NOT TRUST CONTENT OF defragDest TO BE VALID!
    // ONLY lastValidHeader IS GUARANTEED VALID!
    for (StorageHeader *currHeader = poolBegin, *defragDest = poolBegin, *prevHeader = nullptr; currHeader < poolEnd;) {
        // first, consolidate free space
        currHeader->flags &= ~StorageFlags::used;
        const uint32* currData = currHeader->GetDataPtr();

        for (uint32 e = 0; e < storage->entryCount; ++e) {
            if (currData == storage->storageEntries[e]) {
                currHeader->flags |= StorageFlags::used;
                break;
            }
        }

        auto* next = currHeader->VeryUnsafeNext();

        if (!currHeader->IsUsed()) {
            // if we have two adjacent unused blocks then combine them and continue along.
            // note that defragDest is left in place.
            if (prevHeader != nullptr && !prevHeader->IsUsed()) {
                prevHeader->capacity += StorageHeader::SizeInts() + currHeader->capacity;
                assert(prevHeader->VeryUnsafeNext() == next);
            }
            else {
                prevHeader = currHeader;
            }

            currHeader = next;
            continue;
        }

        // if the current header is used, then we don't care what the previous
        // one is anymore; it's only helpful for consolidating adjacent blocks.
        prevHeader = nullptr;

        if (currHeader->IsPinned()) {
            // since allocs before pinned allocs can be moved backwards, we need
            // to make sure we fill the gap between the last non-pinned alloc and
            // the current pinned one.
            if (lastValidHeader != nullptr) {
                assert(lastValidHeader->IsUsed());

                lastValidHeader->capacity = lastValidHeader->length; // reclaim space

                auto* validNext = lastValidHeader->VeryUnsafeNext();
                const size_t delta = reinterpret_cast<size_t>(currHeader) - reinterpret_cast<size_t>(validNext);
                assert(!(delta % sizeof(uint32)));

                if (delta > sizeof(StorageHeader)) {
                    // if there's enough space for a block between the end of the last valid
                    // header and this pinned block, place a free block in that space.
                    *validNext = {};
                    validNext->capacity = (delta - sizeof(StorageHeader)) / sizeof(uint32);
                }
                else {
                    // otherwise, increase capacity to bridge the gap. this excess
                    // can be reclaimed later when the pinned block is freed.
                    lastValidHeader->capacity += delta / sizeof(uint32);
                }

                activeStorage += StorageHeader::SizeInts() + lastValidHeader->capacity;
            }

            lastValidHeader = currHeader;
            currHeader = next;

            // would be complicated to relocate items from after this pinned block
            // to the space before it, so just resume relocation after it instead.
            defragDest = next;
            continue;
        }

        // if somehow the last valid block has excess capacity, reclaim it and
        // move the relocation cursor to that reclaimed space.
        if (lastValidHeader != nullptr) {
            if (lastValidHeader->HasExcess()) {
                lastValidHeader->capacity = lastValidHeader->length;
                defragDest = lastValidHeader->VeryUnsafeNext();
            }

            activeStorage += StorageHeader::SizeInts() + lastValidHeader->capacity;
        }

        if (defragDest < currHeader) {
            relocatedStorage = true;
            lastValidHeader = defragDest;

            const size_t numBytesToMove = sizeof(StorageHeader) + (currHeader->length * sizeof(uint32));
            memmove(defragDest, currHeader, numBytesToMove); // contents of currHeader is now potentially invalid
            assert(defragDest->capacity >= defragDest->length);
            defragDest->capacity = defragDest->length; // reclaim space
            defragDest = defragDest->VeryUnsafeNext();
        }
        else {
            lastValidHeader = currHeader;
            defragDest = next;
        }

        currHeader = next;
    }

    assert(lastValidHeader == nullptr || lastValidHeader->IsUsed());

    if (lastValidHeader != nullptr && lastValidHeader < poolEnd) {
        lastValidHeader->capacity = lastValidHeader->length; // reclaim space

        auto* next = lastValidHeader->VeryUnsafeNext();

        if (next < poolEnd) {
            const size_t delta = reinterpret_cast<size_t>(poolEnd) - reinterpret_cast<size_t>(next);
            assert(!(delta % 4));
            assert(delta > 0 || !relocatedStorage);

            if (delta > sizeof(StorageHeader)) {
                const auto deltaBlockSize = static_cast<uint32>((delta - sizeof(StorageHeader)) / sizeof(uint32));

                if (!relocatedStorage) {
                    assert(next->capacity == deltaBlockSize);
                }

                *next = {}; // wipe all flags to mark as unused
                next->capacity = deltaBlockSize;
            }
            else {
                lastValidHeader->capacity += delta / sizeof(uint32);
            }
        }

        activeStorage += StorageHeader::SizeInts() + lastValidHeader->capacity;
    }

#if RSDK_DEBUG
    uint32 activeStorageEnumerated = 0;

    // sum up used storage by enumerating through the pool to compare against
    // the iterative calculation. this enumerated sum is expected to be correct.
    for (StorageHeader* currHeader = poolBegin;
         currHeader < poolEnd;
         currHeader = currHeader->VeryUnsafeNext()) {
        if (currHeader->IsUsed()) {
            activeStorageEnumerated += StorageHeader::SizeInts() + currHeader->capacity;
        }
    }

    if (activeStorageEnumerated != activeStorage) {
        printf("[GC] [%s] /!\\ ACTIVE STORAGE MISMATCH! EXPECTED: %u; GOT: %u",
               DataSetToString(set), activeStorageEnumerated, activeStorage);
    }
#endif

    if (activeStorage > storage->usedStorage) {
        printf("[GC] [%s] [clearCount: %u] WARNING: GC OVERFLOW!!! %u > %u\n",
               DataSetToString(set), clearCount,
               activeStorage, storage->usedStorage);
    }

    storage->usedStorage = activeStorage;

    if (!relocatedStorage) {
        return;
    }

    for (StorageHeader* currentHeader = poolBegin;
         currentHeader < poolEnd;
         currentHeader = currentHeader->VeryUnsafeNext()) {
        if (!currentHeader->IsUsed()) {
            continue;
        }

        const uint32* pDataNew = currentHeader->GetDataPtr();
        const uint32 newOffset = static_cast<uint32>(reinterpret_cast<uintptr_t>(pDataNew) - reinterpret_cast<uintptr_t>(storage->memoryTable));
        const uint32 oldOffset = currentHeader->poolOffset;

        if (oldOffset == newOffset) {
            continue;
        }

        const uint32 offsetDelta = oldOffset - newOffset;
        const uint32 offsetDelta_i = offsetDelta / sizeof(uint32);
        assert(!(offsetDelta % sizeof(uint32)));
        const void* pDataOld = reinterpret_cast<uint8_t*>(storage->memoryTable) + oldOffset;

        for (uint32 e = 0; e < storage->entryCount; ++e) {
            if (pDataOld == storage->storageEntries[e] && storage->dataEntries[e] != nullptr) {
                storage->storageEntries[e] -= offsetDelta_i;
                *storage->dataEntries[e] -= offsetDelta_i;
            }
        }

        currentHeader->poolOffset = newOffset;
    }
#else
    uint32 processedStorage = 0;
    uint32 unusedStorage    = 0;

    uint32 *defragmentDestination = dataStorage[set].memoryTable;
    uint32 *currentHeader         = dataStorage[set].memoryTable;

    #if RETRO_PLATFORM == RETRO_KALLISTIOS
    const uint32 clearCount = dataStorage[set].clearCount++;
    #else
    ++dataStorage[set].clearCount;
    #endif

    // Perform garbage-collection. This deallocates all memory allocations that are no longer being used.
    GarbageCollectStorage(set);

    // This performs defragmentation. It works by removing 'gaps' between the various blocks of allocated memory,
    // grouping them all together at the start of the buffer while all the empty space goes at the end.
    // Avoiding fragmentation is important, as fragmentation can cause allocations to fail despite there being
    // enough free memory because that free memory isn't contiguous.
    while (processedStorage < dataStorage[set].usedStorage) {
        uint32 *dataPtr = &dataStorage[set].memoryTable[currentHeader[HEADER_DATA_OFFSET]];
        uint32 size     = (currentHeader[HEADER_DATA_LENGTH] / sizeof(uint32)) + HEADER_SIZE;

        // Check if this block of memory is currently allocated.
        currentHeader[HEADER_ACTIVE] = false;

        for (int32 e = 0; e < dataStorage[set].entryCount; ++e)
            if (dataPtr == dataStorage[set].storageEntries[e])
                currentHeader[HEADER_ACTIVE] = true;

        if (currentHeader[HEADER_ACTIVE]) {
            // This memory is being used.
            processedStorage += size;

            if (currentHeader > defragmentDestination) {
                #if RETRO_PLATFORM == RETRO_KALLISTIOS
                printf("[GC] [%s] [clearCount: %u] WARNING: GC RELOCATING HEADER INTS: %u\n", DataSetToString(set), clearCount, size);
                #endif
                // This memory has a gap before it, so move it backwards into that free space.
                for (uint32 i = 0; i < size; ++i) *defragmentDestination++ = *currentHeader++;
            }
            else {
                // This memory doesn't have a gap before it, so we don't need to move it - just skip it instead.
                defragmentDestination += size;
                currentHeader += size;
            }
        }
        else {
            // This memory is not being used, so skip it.
            currentHeader += size;
            processedStorage += size;
            unusedStorage += size;
        }
    }

    // If defragmentation occurred, then we need to update every single
    // pointer to allocated memory to point to their new locations in the buffer.
    if (unusedStorage != 0) {
        // DCWIP
        #if RETRO_PLATFORM == RETRO_KALLISTIOS
        if (unusedStorage > dataStorage[set].usedStorage) {
            printf("[GC] [%s] [clearCount: %u] WARNING: GC OVERFLOW!!!\n", DataSetToString(set), clearCount);
            dataStorage[set].usedStorage = 0;
        } else {
            dataStorage[set].usedStorage -= unusedStorage;
        }
        #else
        dataStorage[set].usedStorage -= unusedStorage;
        #endif

        /*uint32 **/currentHeader = dataStorage[set].memoryTable;

        uint32 dataOffset = 0;
        while (dataOffset < dataStorage[set].usedStorage) {
            uint32 *dataPtr = &dataStorage[set].memoryTable[currentHeader[HEADER_DATA_OFFSET]];
            uint32 size     = (currentHeader[HEADER_DATA_LENGTH] / sizeof(uint32)) + HEADER_SIZE; // size (in int32s)

            // Find every single pointer to this memory allocation and update them with its new address.
            for (int32 c = 0; c < dataStorage[set].entryCount; ++c)

#if !RETRO_USE_ORIGINAL_CODE
                // make sure dataEntries[e] isn't null. If it is null by some ungodly chance then it was prolly already freed or something idk
                if (dataPtr == dataStorage[set].storageEntries[c] && dataStorage[set].dataEntries[c])
                    dataStorage[set].storageEntries[c] = *dataStorage[set].dataEntries[c] = currentHeader + HEADER_SIZE;
#else
                if (dataPtr == dataStorage[set].storageEntries[c])
                    dataStorage[set].storageEntries[c] = *dataStorage[set].dataEntries[c] = currentHeader + HEADER_SIZE;
#endif


            // Update the offset in the allocation's header too.
            currentHeader[HEADER_DATA_OFFSET] = dataOffset + HEADER_SIZE;

            // Advance to the next memory allocation.
            currentHeader += size;
            dataOffset += size;
        }
    }
#endif
}

void RSDK::CopyStorage(uint32 **src, uint32 **dst)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if (dst != NULL) {
        uint32 *dstPtr = *dst;
        *src           = *dst;

        const auto* header = reinterpret_cast<StorageHeader*>(*dstPtr) - 1;
        DataStorage* storage = &dataStorage[header->GetDataSet()];

        if (storage->entryCount < STORAGE_ENTRY_COUNT) {
            storage->dataEntries[storage->entryCount]    = src;
            storage->storageEntries[storage->entryCount] = *src;

            ++storage->entryCount;

            if (storage->entryCount >= STORAGE_ENTRY_COUNT)
                GarbageCollectStorage(static_cast<StorageDataSets>(header->GetDataSet()));
        }
    }
#else
    if (dst != NULL) {
        uint32 *dstPtr = *dst;
        *src           = *dst;

        if (dataStorage[HEADER(dstPtr, HEADER_SET_ID)].entryCount < STORAGE_ENTRY_COUNT) {
            dataStorage[HEADER(dstPtr, HEADER_SET_ID)].dataEntries[dataStorage[HEADER(dstPtr, HEADER_SET_ID)].entryCount]    = src;
            dataStorage[HEADER(dstPtr, HEADER_SET_ID)].storageEntries[dataStorage[HEADER(dstPtr, HEADER_SET_ID)].entryCount] = *src;

            ++dataStorage[HEADER(dstPtr, HEADER_SET_ID)].entryCount;

            if (dataStorage[HEADER(dstPtr, HEADER_SET_ID)].entryCount >= STORAGE_ENTRY_COUNT)
                GarbageCollectStorage((StorageDataSets)HEADER(dstPtr, HEADER_SET_ID));
        }
    }
#endif
}

void RSDK::GarbageCollectStorage(StorageDataSets set)
{
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    if (set >= DATASET_MAX) {
        return;
    }

    DataStorage* storage = &dataStorage[set];

    if (storage->memoryTable == nullptr) {
        return;
    }

    for (uint32 e = 0; e < storage->entryCount; ++e) {
        if (storage->dataEntries[e] != nullptr && *storage->dataEntries[e] != storage->storageEntries[e]) {
            storage->dataEntries[e] = nullptr;
        }
    }

    uint32 newEntryCount = 0;

    for (uint32 e = 0; e < storage->entryCount; ++e) {
        if (storage->dataEntries[e] == nullptr) {
            continue;
        }

        if (e != newEntryCount) {
            storage->dataEntries[newEntryCount] = storage->dataEntries[e];
            storage->storageEntries[newEntryCount] = storage->storageEntries[e];

            storage->dataEntries[e] = nullptr;
            storage->storageEntries[e] = nullptr;
        }

        ++newEntryCount;
    }

    for (uint32 e = newEntryCount; e < storage->entryCount; ++e) {
        storage->dataEntries[e] = nullptr;
        storage->storageEntries[e] = nullptr;
    }

    storage->entryCount = newEntryCount;
#else
    if ((uint32)set < DATASET_MAX) {
        for (uint32 e = 0; e < dataStorage[set].entryCount; ++e) {
            // So what's happening here is the engine is checking to see if the storage entry
            // (which is the pointer to the "memoryTable" offset that is allocated for this entry)
            // matches what the actual variable that allocated the storage is currently pointing to.
            // if they don't match, the storage entry is considered invalid and marked for removal.

#if RETRO_PLATFORM == RETRO_KALLISTIOS
            if (dataStorage[set].dataEntries[e] != NULL && *dataStorage[set].dataEntries[e] != dataStorage[set].storageEntries[e]) {
                printf("[GC] [%s] WARNING: MARKING FOR REMOVAL: e = %u; *dataEntries[e] (%p) != storageEntries[e] (%p)\n",
                    DataSetToString(set),
                    e,
                    *dataStorage[set].dataEntries[e],
                    dataStorage[set].storageEntries[e]);

                dataStorage[set].dataEntries[e] = NULL;
            }
#else
            if (dataStorage[set].dataEntries[e] != NULL && *dataStorage[set].dataEntries[e] != dataStorage[set].storageEntries[e])
                dataStorage[set].dataEntries[e] = NULL;
#endif
        }

        uint32 newEntryCount = 0;
        for (uint32 entryID = 0; entryID < dataStorage[set].entryCount; ++entryID) {
            if (dataStorage[set].dataEntries[entryID]) {
                if (entryID != newEntryCount) {
                    dataStorage[set].dataEntries[newEntryCount]    = dataStorage[set].dataEntries[entryID];
                    dataStorage[set].dataEntries[entryID]          = NULL;
                    dataStorage[set].storageEntries[newEntryCount] = dataStorage[set].storageEntries[entryID];
                    dataStorage[set].storageEntries[entryID]       = NULL;
                }

                ++newEntryCount;
            }
        }
        dataStorage[set].entryCount = newEntryCount;

        for (int32 e = dataStorage[set].entryCount; e < STORAGE_ENTRY_COUNT; ++e) {
            dataStorage[set].dataEntries[e]    = NULL;
            dataStorage[set].storageEntries[e] = NULL;
        }
    }
#endif
}
