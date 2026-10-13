#include "NiMemObject.hpp"
#include "NiMemManager.hpp"

// GAME - 0xAA13E0
// GECK - 0x851B00
void* NiMemObject::operator new(size_t stSize) {
    if (!stSize)
        stSize = 1;

    size_t stAlignment = 4;
    return NiMemManager::GetSingleton()->m_pkAllocator->Allocate(stSize, stAlignment, NI_OPER_NEW, true, 0, -1, 0);
}

// GAME - 0xAA1420
// GECK - 0x851B40
void* NiMemObject::operator new[](size_t stSize) {
    if (!stSize)
        stSize = 1;

    size_t stAlignment = 4;
    return NiMemManager::GetSingleton()->m_pkAllocator->Allocate(stSize, stAlignment, NI_OPER_NEW_ARRAY, false, 0, -1, 0);
}

// GAME - 0xAA1460
// GECK - 0x851B80
void NiMemObject::operator delete(void* pvMem, size_t stElementSize) {
    if (pvMem)
        NiMemManager::GetSingleton()->m_pkAllocator->Deallocate(pvMem, NI_OPER_DELETE, stElementSize);
}

// GAME - 0xAA1490
// GECK - 0x851BB0
void NiMemObject::operator delete[](void* pvMem, size_t stElementSize) {
    if (pvMem)
        NiMemManager::GetSingleton()->m_pkAllocator->Deallocate(pvMem, NI_OPER_DELETE_ARRAY, UINT32_MAX);
}
