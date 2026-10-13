#include "NiMemory.hpp"
#include "NiMemManager.hpp"

// GAME - 0xAA13E0
// GECK - 0x851B00
void* NiNew(size_t stSize) {
    if (!stSize)
        stSize = 1;

    size_t stAlignment = 4;
    return NiMemManager::GetSingleton()->m_pkAllocator->Allocate(stSize, stAlignment, NI_OPER_NEW, true, 0, -1, 0);
}

// GAME - 0xAA1070
// GECK - 0x851790
void* NiAlloc(size_t stSize) {
    if (!stSize)
        stSize = 1;

    size_t stAlignment = 4;
    return NiMemManager::GetSingleton()->m_pkAllocator->Allocate(stSize, stAlignment, NI_MALLOC, false, 0, -1, 0);
}

// GAME - 0xAA10B0
// GECK - 0x8517D0
void* NiAlignedAlloc(size_t stSize, size_t stAlignment) {
    if (!stSize)
        stSize = 1;

    return NiMemManager::GetSingleton()->m_pkAllocator->Allocate(stSize, stAlignment, NI_ALIGNEDMALLOC, false, 0, -1, 0);
}

// GAME - 0xAA10F0
// GECK - 0x851810
void NiFree(void* pvMem) {
    if (pvMem)
        NiMemManager::GetSingleton()->m_pkAllocator->Deallocate(pvMem, NI_FREE, UINT32_MAX);
}

// GAME - 0xAA1110
// GECK - 0x851830
void NiAlignedFree(void* pvMem) {
    if (pvMem)
        NiMemManager::GetSingleton()->m_pkAllocator->Deallocate(pvMem, NI_ALIGNEDFREE, UINT32_MAX);
}

// GAME - 0xAA1460
// GECK - 0x851B80
void NiDelete(void* pvMem, size_t stElementSize) {
    if (pvMem)
        NiMemManager::GetSingleton()->m_pkAllocator->Deallocate(pvMem, NI_OPER_DELETE, stElementSize);
}
