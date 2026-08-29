#include "ProbePlugin.hpp"
#include "RoleDefinitions.hpp"
#include <nullclap/Entry.hpp>
using Probe = nullclap::midi_role_probe::ProbePlugin<nullclap::midi_role_probe::audioNoteEffectId, nullclap::midi_role_probe::audioNoteEffectName, nullclap::midi_role_probe::audioNoteEffectFeatures>;
NULLCLAP_DEFINE_ENTRY(Probe);
