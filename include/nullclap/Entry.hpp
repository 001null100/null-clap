#pragma once

#include "Factory.hpp"

#include <clap/entry.h>
#include <cstdint>
#include <cstring>
#include <mutex>

// Define this macro exactly once in a plug-in module's Entry.cpp.
#define NULLCLAP_DEFINE_ENTRY(PluginType)                                                       \
    namespace                                                                                   \
    {                                                                                           \
    std::mutex nullclapEntryMutex;                                                              \
    std::uint32_t nullclapEntryRefCount = 0;                                                    \
                                                                                                \
    bool CLAP_ABI nullclapEntryInit(const char*)                                                \
    {                                                                                           \
        const std::scoped_lock lock(nullclapEntryMutex);                                        \
        ++nullclapEntryRefCount;                                                                \
        return true;                                                                            \
    }                                                                                           \
                                                                                                \
    void CLAP_ABI nullclapEntryDeinit()                                                         \
    {                                                                                           \
        const std::scoped_lock lock(nullclapEntryMutex);                                        \
        if (nullclapEntryRefCount > 0)                                                          \
            --nullclapEntryRefCount;                                                            \
    }                                                                                           \
                                                                                                \
    const void* CLAP_ABI nullclapGetFactory(const char* factoryId)                              \
    {                                                                                           \
        if (factoryId != nullptr && std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0)         \
            return nullclap::SinglePluginFactory<PluginType>::get();                            \
        return nullptr;                                                                         \
    }                                                                                           \
    }                                                                                           \
                                                                                                \
    extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {                            \
        CLAP_VERSION,                                                                           \
        &nullclapEntryInit,                                                                     \
        &nullclapEntryDeinit,                                                                   \
        &nullclapGetFactory,                                                                    \
    }
