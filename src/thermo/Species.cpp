// This file is part of Cantera. See License.txt in the top-level directory or
// at https://cantera.org/license.txt for license and copyright information.

#include "cantera/thermo/Species.h"
#include "cantera/thermo/SpeciesThermoInterpType.h"
#include "cantera/thermo/SpeciesThermoFactory.h"
#include "cantera/thermo/ThermoPhase.h"
#include "cantera/transport/TransportData.h"
#include "cantera/base/stringUtils.h"
#include "cantera/base/ctexceptions.h"
#include "cantera/base/global.h"
#include <iostream>
#include <limits>
#include <set>

namespace Cantera {

Species::Species()
    : charge(0.0)
    , size(1.0)
{
}

Species::Species(const std::string& name_, const compositionMap& comp_,
                 double charge_, double size_)
    : name(name_)
    , composition(comp_)
    , charge(charge_)
    , size(size_)
{
}

Species::~Species()
{
}

AnyMap Species::parameters(const ThermoPhase* phase, bool withInput) const
{
    AnyMap speciesNode;
    speciesNode["name"] = name;
    speciesNode["composition"] = composition;
    speciesNode["composition"].setFlowStyle();

    if (charge != 0) {
        speciesNode["charge"] = charge;
    }
    if (size != 1) {
        speciesNode["size"] = size;
    }
    if (thermo) {
        AnyMap thermoNode = thermo->parameters(withInput);
        if (thermoNode.size()) {
            speciesNode["thermo"] = std::move(thermoNode);
        }
    }
    if (transport) {
        speciesNode["transport"] = transport->parameters(withInput);
    }
    if (phase) {
        phase->getSpeciesParameters(name, speciesNode);
    }
    if (withInput && input.hasKey("equation-of-state")) {
        auto& eosIn = input["equation-of-state"].asVector<AnyMap>();
        for (const auto& eos : eosIn) {
            auto& out = speciesNode["equation-of-state"].getMapWhere(
                "model", eos["model"].asString(), true);
            out.update(eos);
        }
    }
    if (withInput) {
        speciesNode.update(input);
    }
    return speciesNode;
}

unique_ptr<Species> newSpecies(const AnyMap& node)
{
    unique_ptr<Species> s(new Species(node["name"].asString(),
                                      node["composition"].asMap<double>()));

    if (node.hasKey("thermo")) {
        s->thermo = newSpeciesThermo(node["thermo"].as<AnyMap>());
    } else {
        s->thermo.reset(new SpeciesThermoInterpType());
    }

    s->size = node.getDouble("sites", 1.0);
    if (s->composition.find("E") != s->composition.end()) {
        s->charge = -s->composition["E"];
    }

    if (node.hasKey("transport")) {
        s->transport = newTransportData(node["transport"].as<AnyMap>());
        s->transport->validate(*s);
    }

    // Store input parameters in the "input" map, unless they are stored in a
    // child object
    const static std::set<std::string> known_keys{
        "thermo", "transport"
    };
    s->input.setUnits(node.units());
    for (const auto& item : node) {
        if (known_keys.count(item.first) == 0) {
            s->input[item.first] = item.second;
        }
    }
    s->input.applyUnits();
    s->input.copyMetadata(node);

    return s;
}

std::vector<shared_ptr<Species>> getSpecies(const AnyValue& items)
{
    std::vector<shared_ptr<Species> > all_species;
    for (const auto& node : items.asVector<AnyMap>()) {
        all_species.emplace_back(newSpecies(node));
    }
    return all_species;
}

}
