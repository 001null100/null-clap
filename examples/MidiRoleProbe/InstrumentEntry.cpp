#include "ProbePlugin.hpp"
#include "RoleDefinitions.hpp"
#include <nullclap/Entry.hpp>
using Probe = nullclap::midi_role_probe::ProbePlugin<nullclap::midi_role_probe::instrumentId, nullclap::midi_role_probe::instrumentName, nullclap::midi_role_probe::instrumentFeatures>;
NULLCLAP_DEFINE_ENTRY(Probe);
