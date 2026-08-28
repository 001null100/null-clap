#pragma once

#include <clap/ext/params.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nullclap
{
struct ParameterSpec
{
    clap_id id = CLAP_INVALID_ID;
    std::string name;
    std::string module;
    double minimum = 0.0;
    double maximum = 1.0;
    double defaultValue = 0.0;
    std::uint32_t flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
    std::string unit;
    int displayPrecision = 3;
    std::vector<std::string> valueLabels;
    bool persistent = true;

    static ParameterSpec continuous(clap_id id,
                                    std::string name,
                                    std::string module,
                                    double minimum,
                                    double maximum,
                                    double defaultValue,
                                    std::uint32_t flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE)
    {
        ParameterSpec spec;
        spec.id = id;
        spec.name = std::move(name);
        spec.module = std::move(module);
        spec.minimum = minimum;
        spec.maximum = maximum;
        spec.defaultValue = defaultValue;
        spec.flags = flags;
        return spec;
    }

    static ParameterSpec choice(clap_id id,
                                std::string name,
                                std::string module,
                                std::vector<std::string> labels,
                                std::size_t defaultIndex = 0,
                                std::uint32_t flags = CLAP_PARAM_IS_AUTOMATABLE)
    {
        ParameterSpec spec;
        spec.id = id;
        spec.name = std::move(name);
        spec.module = std::move(module);
        spec.minimum = 0.0;
        spec.maximum = labels.empty() ? 0.0 : static_cast<double>(labels.size() - 1);
        spec.defaultValue = labels.empty() ? 0.0 : static_cast<double>(defaultIndex < labels.size() ? defaultIndex : 0);
        spec.flags = flags | CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
        spec.displayPrecision = 0;
        spec.valueLabels = std::move(labels);
        return spec;
    }
};
} // namespace nullclap
