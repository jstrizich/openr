/**
 * Copyright (c) 2014-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "openr/decision/PrefixState.h"

#include <openr/common/Util.h>

using apache::thrift::can_throw;

namespace openr {

std::unordered_set<thrift::IpPrefix>
PrefixState::updatePrefix(
    PrefixKey const& key, thrift::PrefixEntry const& entry) {
  std::unordered_set<thrift::IpPrefix> changed;
  auto const nodeAndArea =
      std::make_pair(key.getNodeName(), key.getPrefixArea());

  // Update our state with this newly advertised entry
  nodeToPrefixes_[nodeAndArea].insert(key.getIpPrefix());
  auto [it, inserted] =
      prefixes_[key.getIpPrefix()].emplace(nodeAndArea, entry);

  // Skip rest of code, if prefix exists and has no change
  if (not inserted && it->second == entry) {
    return changed;
  }
  // Update prefix
  if (not inserted) {
    it->second = entry;
  }
  changed.insert(key.getIpPrefix());

  VLOG(1) << "[ROUTE ADVERTISEMENT] "
          << "Area: " << key.getPrefixArea() << ", Node: " << key.getNodeName()
          << ", " << toString(entry, VLOG_IS_ON(1));
  return changed;
}

std::unordered_set<thrift::IpPrefix>
PrefixState::deletePrefix(PrefixKey const& key) {
  std::unordered_set<thrift::IpPrefix> changed;
  auto const nodeAndArea =
      std::make_pair(key.getNodeName(), key.getPrefixArea());
  if (nodeToPrefixes_[nodeAndArea].erase(key.getIpPrefix())) {
    changed.insert(key.getIpPrefix());
    VLOG(1) << "[ROUTE WITHDRAW] "
            << "Area: " << key.getPrefixArea()
            << ", Node: " << key.getNodeName() << ", "
            << toString(key.getIpPrefix());
    prefixes_[key.getIpPrefix()].erase(nodeAndArea);
    // clean up data structures
    if (nodeToPrefixes_[nodeAndArea].empty()) {
      nodeToPrefixes_.erase(nodeAndArea);
    }
    if (prefixes_[key.getIpPrefix()].empty()) {
      prefixes_.erase(key.getIpPrefix());
    }
  }
  return changed;
}

std::unordered_map<std::string /* nodeName */, thrift::PrefixDatabase>
PrefixState::getPrefixDatabases() const {
  std::unordered_map<std::string, thrift::PrefixDatabase> prefixDatabases;
  for (auto const& [nodeAndArea, prefixes] : nodeToPrefixes_) {
    thrift::PrefixDatabase prefixDb;
    *prefixDb.thisNodeName_ref() = nodeAndArea.first;
    prefixDb.area_ref() = nodeAndArea.second;
    for (auto const& prefix : prefixes) {
      prefixDb.prefixEntries_ref()->emplace_back(
          prefixes_.at(prefix).at(nodeAndArea));
    }
    prefixDatabases.emplace(nodeAndArea.first, std::move(prefixDb));
  }
  return prefixDatabases;
}

std::vector<thrift::ReceivedRouteDetail>
PrefixState::getReceivedRoutesFiltered(
    thrift::ReceivedRouteFilter const& filter) const {
  std::vector<thrift::ReceivedRouteDetail> routes;
  if (filter.prefixes_ref()) {
    for (auto& prefix : filter.prefixes_ref().value()) {
      auto it = prefixes_.find(prefix);
      if (it == prefixes_.end()) {
        continue;
      }
      filterAndAddReceivedRoute(
          routes,
          filter.nodeName_ref(),
          filter.areaName_ref(),
          it->first,
          it->second);
    }
  } else {
    for (auto& [prefix, prefixEntries] : prefixes_) {
      filterAndAddReceivedRoute(
          routes,
          filter.nodeName_ref(),
          filter.areaName_ref(),
          prefix,
          prefixEntries);
    }
  }
  return routes;
}

void
PrefixState::filterAndAddReceivedRoute(
    std::vector<thrift::ReceivedRouteDetail>& routes,
    apache::thrift::optional_field_ref<const std::string&> const& nodeFilter,
    apache::thrift::optional_field_ref<const std::string&> const& areaFilter,
    thrift::IpPrefix const& prefix,
    PrefixEntries const& prefixEntries) {
  // Return immediately if no prefix-entry
  if (prefixEntries.empty()) {
    return;
  }

  thrift::ReceivedRouteDetail routeDetail;
  routeDetail.prefix_ref() = prefix;

  // Add prefix entries and honor the filter
  for (auto& [nodeAndArea, prefixEntry] : prefixEntries) {
    if (nodeFilter && *nodeFilter != nodeAndArea.first) {
      continue;
    }
    if (areaFilter && *areaFilter != nodeAndArea.second) {
      continue;
    }
    routeDetail.routes_ref()->emplace_back();
    auto& route = routeDetail.routes_ref()->back();
    route.key_ref()->node_ref() = nodeAndArea.first;
    route.key_ref()->area_ref() = nodeAndArea.second;
    route.route_ref() = prefixEntry;
  }

  // Add detail if there are entries to return
  if (routeDetail.routes_ref()->size()) {
    routes.emplace_back(std::move(routeDetail));
  }
}

bool
PrefixState::hasConflictingForwardingInfo(const PrefixEntries& prefixEntries) {
  // Empty prefix entries doesn't indicate conflicting information
  if (prefixEntries.empty()) {
    return false;
  }

  // There is at-least one entry, get its reference
  const auto& firstEntry = prefixEntries.begin()->second;

  // Iterate over all entries and make sure the forwarding information agrees
  for (auto& [_, entry] : prefixEntries) {
    if (firstEntry.forwardingAlgorithm_ref() !=
        entry.forwardingAlgorithm_ref()) {
      return true;
    }
    if (firstEntry.forwardingType_ref() != entry.forwardingType_ref()) {
      return true;
    }
  }

  // No conflicting information
  return false;
}

} // namespace openr
