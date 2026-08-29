#include "ProbePlugin.hpp"
#include "RoleDefinitions.hpp"
#include <nullclap/Entry.hpp>
using Probe = nullclap::midi_role_probe::ProbePlugin<nullclap::midi_role_probe::audioEffectId, nullclap::midi_role_probe::audioEffectName, nullclap::midi_role_probe::audioEffectFeatures>;
NULLCLAP_DEFINE_ENTRY(Probe);
