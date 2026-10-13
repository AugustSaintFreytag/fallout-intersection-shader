#include "NiMemManager.hpp"

NiMemManager* NiMemManager::GetSingleton() {
    return *reinterpret_cast<NiMemManager**>(0x11F6080);
}
