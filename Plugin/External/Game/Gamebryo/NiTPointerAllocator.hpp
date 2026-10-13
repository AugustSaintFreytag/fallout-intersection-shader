#pragma once

#include "NiTAbstractPoolAllocator.hpp"

template <class T_Data>
class NiTPointerAllocator : public NiTAbstractPoolAllocator<T_Data> {
public:
    __declspec(restrict) void* Allocate() {
        return CdeclCall<void*>(0x43A010);
    };

    void Deallocate(void* apData) {
        ThisCall(0x45CEE0, this, apData);
    };
};
