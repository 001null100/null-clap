#include <nullclap/ParameterStore.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nullclap
{
namespace
{
void copyClapString(char* destination, std::size_t capacity, const std::string& source) noexcept
{
    if (capacity == 0)
        return;
    std::strncpy(destination, source.c_str(), capacity - 1);
    destination[capacity - 1] = '\0';
}
}

bool ParameterStore::add(ParameterSpec spec)
{
    if (spec.id == CLAP_INVALID_ID || spec.name.empty() || spec.maximum < spec.minimum || byId_.contains(spec.id))
        return false;

    spec.defaultValue = clampAndQuantize(spec, spec.defaultValue);
    auto parameter = std::make_unique<Parameter>(std::move(spec));
    auto* pointer = parameter.get();
    byId_.emplace(pointer->spec.id, pointer);
    parameters_.push_back(std::move(parameter));
    return true;
}

std::uint32_t ParameterStore::count() const noexcept
{
    return static_cast<std::uint32_t>(parameters_.size());
}

bool ParameterStore::info(std::uint32_t index, clap_param_info_t& out) const noexcept
{
    if (index >= parameters_.size())
        return false;

    const auto& parameter = *parameters_[index];
    out = {};
    out.id = parameter.spec.id;
    out.flags = parameter.spec.flags;
    out.cookie = const_cast<Parameter*>(&parameter);
    copyClapString(out.name, CLAP_NAME_SIZE, parameter.spec.name);
    copyClapString(out.module, CLAP_PATH_SIZE, parameter.spec.module);
    out.min_value = parameter.spec.minimum;
    out.max_value = parameter.spec.maximum;
    out.default_value = parameter.spec.defaultValue;
    return true;
}

bool ParameterStore::contains(clap_id id) const noexcept
{
    return find(id) != nullptr;
}

bool ParameterStore::isReadOnly(clap_id id) const noexcept
{
    const auto* parameter = find(id);
    return parameter != nullptr && (parameter->spec.flags & CLAP_PARAM_IS_READONLY) != 0;
}

double ParameterStore::value(clap_id id) const noexcept
{
    const auto* parameter = find(id);
    return parameter != nullptr ? parameter->base.load(std::memory_order_relaxed) : 0.0;
}

double ParameterStore::modulation(clap_id id) const noexcept
{
    const auto* parameter = find(id);
    return parameter != nullptr ? parameter->modulationAmount.load(std::memory_order_relaxed) : 0.0;
}

double ParameterStore::effectiveValue(clap_id id) const noexcept
{
    const auto* parameter = find(id);
    if (parameter == nullptr)
        return 0.0;

    const double base = parameter->base.load(std::memory_order_relaxed);
    const double mod = parameter->modulationAmount.load(std::memory_order_relaxed);
    return std::clamp(base + mod, parameter->spec.minimum, parameter->spec.maximum);
}

bool ParameterStore::setBaseValue(clap_id id, double newValue, bool allowReadOnly) noexcept
{
    auto* parameter = find(id);
    if (parameter == nullptr || (!allowReadOnly && (parameter->spec.flags & CLAP_PARAM_IS_READONLY) != 0))
        return false;

    parameter->base.store(clampAndQuantize(parameter->spec, newValue), std::memory_order_relaxed);
    return true;
}

bool ParameterStore::setInternalValue(clap_id id, double newValue) noexcept
{
    return setBaseValue(id, newValue, true);
}

bool ParameterStore::setModulation(clap_id id, double amount) noexcept
{
    auto* parameter = find(id);
    if (parameter == nullptr || (parameter->spec.flags & CLAP_PARAM_IS_MODULATABLE) == 0)
        return false;

    parameter->modulationAmount.store(amount, std::memory_order_relaxed);
    return true;
}

bool ParameterStore::valueToText(clap_id id, double rawValue, char* display, std::uint32_t size) const noexcept
{
    const auto* parameter = find(id);
    if (parameter == nullptr || display == nullptr || size == 0)
        return false;

    const double value = clampAndQuantize(parameter->spec, rawValue);
    if (!parameter->spec.valueLabels.empty())
    {
        const auto index = static_cast<std::size_t>(std::llround(value - parameter->spec.minimum));
        if (index < parameter->spec.valueLabels.size())
        {
            std::snprintf(display, size, "%s", parameter->spec.valueLabels[index].c_str());
            return true;
        }
    }

    const int precision = std::clamp(parameter->spec.displayPrecision, 0, 9);
    if (parameter->spec.unit.empty())
        std::snprintf(display, size, "%.*f", precision, value);
    else
        std::snprintf(display, size, "%.*f %s", precision, value, parameter->spec.unit.c_str());
    return true;
}

bool ParameterStore::textToValue(clap_id id, const char* display, double& outValue) const noexcept
{
    const auto* parameter = find(id);
    if (parameter == nullptr || display == nullptr)
        return false;

    for (std::size_t index = 0; index < parameter->spec.valueLabels.size(); ++index)
    {
        if (parameter->spec.valueLabels[index] == display)
        {
            outValue = parameter->spec.minimum + static_cast<double>(index);
            return true;
        }
    }

    char* end = nullptr;
    const double parsed = std::strtod(display, &end);
    if (end == display || !std::isfinite(parsed))
        return false;

    outValue = clampAndQuantize(parameter->spec, parsed);
    return true;
}

bool ParameterStore::applyInputEvent(const clap_event_header_t& event) noexcept
{
    if (event.space_id != CLAP_CORE_EVENT_SPACE_ID)
        return false;

    if (event.type == CLAP_EVENT_PARAM_VALUE && event.size >= sizeof(clap_event_param_value_t))
    {
        const auto& valueEvent = reinterpret_cast<const clap_event_param_value_t&>(event);
        if (!isGlobalTarget(valueEvent.note_id, valueEvent.port_index, valueEvent.channel, valueEvent.key))
            return false;
        return setBaseValue(valueEvent.param_id, valueEvent.value, false);
    }

    if (event.type == CLAP_EVENT_PARAM_MOD && event.size >= sizeof(clap_event_param_mod_t))
    {
        const auto& modEvent = reinterpret_cast<const clap_event_param_mod_t&>(event);
        if (!isGlobalTarget(modEvent.note_id, modEvent.port_index, modEvent.channel, modEvent.key))
            return false;
        return setModulation(modEvent.param_id, modEvent.amount);
    }

    if ((event.type == CLAP_EVENT_PARAM_GESTURE_BEGIN || event.type == CLAP_EVENT_PARAM_GESTURE_END)
        && event.size >= sizeof(clap_event_param_gesture_t))
        return true;

    return false;
}

std::vector<ParameterStore::SavedValue> ParameterStore::persistentValues() const
{
    std::vector<SavedValue> result;
    result.reserve(parameters_.size());
    for (const auto& parameter : parameters_)
        if (parameter->spec.persistent)
            result.push_back({ parameter->spec.id, parameter->base.load(std::memory_order_relaxed) });
    return result;
}

bool ParameterStore::restorePersistentValue(clap_id id, double restoredValue) noexcept
{
    auto* parameter = find(id);
    if (parameter == nullptr || !parameter->spec.persistent)
        return false;
    return setBaseValue(id, restoredValue, true);
}

ParameterStore::Parameter* ParameterStore::find(clap_id id) noexcept
{
    const auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

const ParameterStore::Parameter* ParameterStore::find(clap_id id) const noexcept
{
    const auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

bool ParameterStore::isGlobalTarget(int32_t noteId, int16_t port, int16_t channel, int16_t key) noexcept
{
    return noteId == -1 && port == -1 && channel == -1 && key == -1;
}

double ParameterStore::clampAndQuantize(const ParameterSpec& spec, double value) noexcept
{
    if (!std::isfinite(value))
        value = spec.defaultValue;
    value = std::clamp(value, spec.minimum, spec.maximum);
    if ((spec.flags & CLAP_PARAM_IS_STEPPED) != 0)
        value = std::round(value);
    return std::clamp(value, spec.minimum, spec.maximum);
}
} // namespace nullclap
