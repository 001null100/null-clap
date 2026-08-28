#pragma once

#include "Parameter.hpp"

#include <atomic>
#include <clap/events.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nullclap
{
class ParameterStore
{
public:
    struct SavedValue
    {
        clap_id id = CLAP_INVALID_ID;
        double value = 0.0;
    };

    ParameterStore() = default;
    ParameterStore(const ParameterStore&) = delete;
    ParameterStore& operator=(const ParameterStore&) = delete;

    bool add(ParameterSpec spec);
    std::uint32_t count() const noexcept;
    bool info(std::uint32_t index, clap_param_info_t& out) const noexcept;

    bool contains(clap_id id) const noexcept;
    bool isReadOnly(clap_id id) const noexcept;
    double value(clap_id id) const noexcept;
    double modulation(clap_id id) const noexcept;
    double effectiveValue(clap_id id) const noexcept;

    bool setBaseValue(clap_id id, double value, bool allowReadOnly = false) noexcept;
    bool setInternalValue(clap_id id, double value) noexcept;
    bool setModulation(clap_id id, double amount) noexcept;

    bool valueToText(clap_id id, double value, char* display, std::uint32_t size) const noexcept;
    bool textToValue(clap_id id, const char* display, double& value) const noexcept;

    // Applies global CLAP parameter events. Polyphonic parameter events return false
    // and remain available to the plug-in's raw event hook.
    bool applyInputEvent(const clap_event_header_t& event) noexcept;

    std::vector<SavedValue> persistentValues() const;
    bool restorePersistentValue(clap_id id, double value) noexcept;

private:
    struct Parameter
    {
        explicit Parameter(ParameterSpec s)
            : spec(std::move(s)), base(spec.defaultValue), modulationAmount(0.0)
        {
        }

        ParameterSpec spec;
        std::atomic<double> base;
        std::atomic<double> modulationAmount;
    };

    Parameter* find(clap_id id) noexcept;
    const Parameter* find(clap_id id) const noexcept;
    static bool isGlobalTarget(int32_t noteId, int16_t port, int16_t channel, int16_t key) noexcept;
    static double clampAndQuantize(const ParameterSpec& spec, double value) noexcept;

    std::vector<std::unique_ptr<Parameter>> parameters_;
    std::unordered_map<clap_id, Parameter*> byId_;
};
} // namespace nullclap
