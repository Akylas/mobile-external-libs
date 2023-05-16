#include "sif/pedestriancost.h"
#include "sif/costconstants.h"

#include "baldr/accessrestriction.h"
#include "baldr/graphconstants.h"
#include "midgard/constants.h"
#include "midgard/util.h"

#ifdef INLINE_TEST
#include "test/test.h"
#include "worker.h"
#include <random>
#endif

using namespace valhalla::midgard;
using namespace valhalla::baldr;

namespace valhalla {
namespace sif {

// Default options/values
namespace {

// Base transition costs
// TODO - can we define these in dynamiccost.h and override here if they differ?
constexpr float kDefaultDestinationOnlyPenalty = 600.0f; // Seconds
constexpr float kDefaultManeuverPenalty = 5.0f;          // Seconds
constexpr float kDefaultAlleyPenalty = 5.0f;             // Seconds
constexpr float kDefaultGatePenalty = 10.0f;             // Seconds
constexpr float kDefaultTollBoothCost = 15.0f;           // Seconds
constexpr float kDefaultFerryCost = 300.0f;              // Seconds
constexpr float kDefaultCountryCrossingCost = 600.0f;    // Seconds
constexpr float kDefaultCountryCrossingPenalty = 0.0f;   // Seconds

// Maximum route distances
constexpr uint32_t kMaxDistanceFoot = 100000;      // 100 km
constexpr uint32_t kMaxDistanceWheelchair = 10000; // 10 km

// Default speeds
constexpr float kDefaultSpeedFoot = 5.1f;       // 3.16 MPH
constexpr float kDefaultSpeedWheelchair = 4.0f; // 2.5  MPH  TODO

// Penalty to take steps
constexpr float kDefaultStepPenaltyFoot = 30.0f;        // 30 seconds
constexpr float kDefaultStepPenaltyWheelchair = 600.0f; // 10 minutes

// Maximum grade30
constexpr uint32_t kDefaultMaxGradeFoot = 90;
constexpr uint32_t kDefaultMaxGradeWheelchair = 12; // Conservative for now...

// Other defaults (not dependent on type)
constexpr uint8_t kDefaultMaxHikingDifficulty = 1; // T1 (kHiking)
constexpr float kModeFactor = 1.5f;                // Favor this mode?
constexpr float kDefaultWalkwayFactor = 1.0f;      // Neutral value for walkways
constexpr float kDefaultSideWalkFactor = 1.0f;     // Neutral value for sidewalks
constexpr float kDefaultAlleyFactor = 2.0f;        // Avoid alleys
constexpr float kDefaultDrivewayFactor = 5.0f;     // Avoid driveways
constexpr float kDefaultUseFerry = 1.0f;
constexpr float kDefaultUseRoad = 0.25f;          // Factor between 0 and 1

// Maximum distance at the beginning or end of a multimodal route
// that you are willing to travel for this mode.  In this case,
// it is the max walking distance.
constexpr uint32_t kTransitStartEndMaxDistance = 2415; // 1.5 miles

// Maximum transfer distance between stops that you are willing
// to travel for this mode.  In this case, it is the max walking
// distance you are willing to walk between transfers.
constexpr uint32_t kTransitTransferMaxDistance = 805; // 0.5 miles

// Avoid roundabouts
constexpr float kRoundaboutFactor = 2.0f;

// Minimum and maximum average pedestrian speed (to validate input).
constexpr float kMinPedestrianSpeed = 0.5f;
constexpr float kMaxPedestrianSpeed = 25.0f;

// Crossing penalties. TODO - may want to lower stop impact when
// 2 cycleways or walkways cross
constexpr uint32_t kCrossingCosts[] = {0, 0, 1, 1, 2, 3, 5, 15};

constexpr float kMinFactor = 0.1f;
constexpr float kMaxFactor = 100000.0f;

// User propensity to use "hilly" roads. Ranges from a value of 0 (avoid
// hills) to 1 (take hills when they offer a more direct, less time, path).
constexpr float kDefaultUseHills = 0.25f;

// Weighting factor based on road class. These apply penalties to higher class
// roads. These penalties are modulated by the useroads factor - further
// avoiding higher class roads for those with low propensity for using roads.
constexpr float kRoadClassFactor[] = {
    1.0f,  // Motorway
    0.4f,  // Trunk
    0.2f,  // Primary
    0.1f,  // Secondary
    0.05f, // Tertiary
    0.05f, // Unclassified
    0.0f,  // Residential
    0.5f   // Service, other
};

// Valid ranges and defaults
constexpr ranged_default_t<uint32_t> kMaxDistanceWheelchairRange{0, kMaxDistanceWheelchair,
                                                                 kMaxDistanceFoot};
constexpr ranged_default_t<uint32_t> kMaxDistanceFootRange{0, kMaxDistanceFoot, kMaxDistanceFoot};

constexpr ranged_default_t<float> kSpeedWheelchairRange{kMinPedestrianSpeed, kDefaultSpeedWheelchair,
                                                        kMaxPedestrianSpeed};
constexpr ranged_default_t<float> kSpeedFootRange{kMinPedestrianSpeed, kDefaultSpeedFoot,
                                                  kMaxPedestrianSpeed};

constexpr ranged_default_t<float> kStepPenaltyWheelchairRange{0, kDefaultStepPenaltyWheelchair,
                                                              kMaxPenalty};
constexpr ranged_default_t<float> kStepPenaltyFootRange{0, kDefaultStepPenaltyFoot, kMaxPenalty};

constexpr ranged_default_t<uint32_t> kMaxGradeWheelchairRange{0, kDefaultMaxGradeWheelchair,
                                                              kDefaultMaxGradeFoot};
constexpr ranged_default_t<uint32_t> kMaxGradeFootRange{0, kDefaultMaxGradeFoot,
                                                        kDefaultMaxGradeFoot};

// Other valid ranges and defaults (not dependent on type)
constexpr ranged_default_t<uint8_t> kMaxHikingDifficultyRange{0, kDefaultMaxHikingDifficulty, 6};
constexpr ranged_default_t<float> kModeFactorRange{kMinFactor, kModeFactor, kMaxFactor};
constexpr ranged_default_t<float> kManeuverPenaltyRange{kMinFactor, kDefaultManeuverPenalty,
                                                        kMaxPenalty};
constexpr ranged_default_t<float> kDestinationOnlyPenaltyRange{0, kDefaultDestinationOnlyPenalty,
                                                               kMaxPenalty};
constexpr ranged_default_t<float> kAlleyPenaltyRange{0.0f, kDefaultAlleyPenalty, kMaxPenalty};
constexpr ranged_default_t<float> kGatePenaltyRange{0.0f, kDefaultGatePenalty, kMaxPenalty};
constexpr ranged_default_t<float> kWalkwayFactorRange{kMinFactor, kDefaultWalkwayFactor, kMaxFactor};
constexpr ranged_default_t<float> kSideWalkFactorRange{kMinFactor, kDefaultSideWalkFactor,
                                                       kMaxFactor};
constexpr ranged_default_t<float> kAlleyFactorRange{kMinFactor, kDefaultAlleyFactor, kMaxFactor};
constexpr ranged_default_t<float> kDrivewayFactorRange{kMinFactor, kDefaultDrivewayFactor,
                                                       kMaxFactor};
constexpr ranged_default_t<float> kFerryCostRange{0, kDefaultFerryCost, kMaxPenalty};
constexpr ranged_default_t<float> kCountryCrossingCostRange{0, kDefaultCountryCrossingCost,
                                                            kMaxPenalty};
constexpr ranged_default_t<float> kCountryCrossingPenaltyRange{0, kDefaultCountryCrossingPenalty,
                                                               kMaxPenalty};
constexpr ranged_default_t<uint32_t> kTransitStartEndMaxDistanceRange{0, kTransitStartEndMaxDistance,
                                                                      100000}; // Max 100k
constexpr ranged_default_t<uint32_t> kTransitTransferMaxDistanceRange{0, kTransitTransferMaxDistance,
                                                                      50000}; // Max 50k
constexpr ranged_default_t<float> kUseFerryRange{0, kDefaultUseFerry, 1.0f};
constexpr ranged_default_t<float> kUseRoadRange{0.0f, kDefaultUseRoad, 1.0f};
constexpr ranged_default_t<float> kUseHillsRange{0.0f, kDefaultUseHills, 1.0f};

constexpr float kSacScaleSpeedFactor[] = {
    1.0f,  // kNone
    1.11f, // kHiking (~90% speed)
    1.25f, // kMountainHiking (80% speed)
    1.54f, // kDemandingMountainHiking (~65% speed)
    2.5f,  // kAlpineHiking (40% speed)
    4.0f,  // kDemandingAlpineHiking (25% speed)
    6.67f  // kDifficultAlpineHiking (~15% speed)
};

constexpr float kSacScaleCostFactor[] = {
    0.0f,  // kNone
    0.25f, // kHiking
    0.75f, // kMountainHiking
    1.25f, // kDemandingMountainHiking
    2.0f,  // kAlpineHiking
    2.5f,  // kDemandingAlpineHiking
    3.0f   // kDifficultAlpineHiking
};

// Speed adjustment factors based on weighted grade. Comments here show an
// example of speed changes based on "grade", using a base speed of 18 MPH
// on flat roads
constexpr float kGradeBasedSpeedFactor[] = {
    2.2f,  // -10%  - 39.6
    2.0f,  // -8%   - 36
    1.9f,  // -6.5% - 34.2
    1.7f,  // -5%   - 30.6
    1.4f,  // -3%   - 25
    1.2f,  // -1.5% - 21.6
    1.0f,  // 0%    - 18
    0.95f, // 1.5%  - 17
    0.85f, // 3%    - 15
    0.75f, // 5%    - 13.5
    0.65f, // 6.5%  - 12
    0.55f, // 8%    - 10
    0.5f,  // 10%   - 9
    0.45f, // 11.5% - 8
    0.4f,  // 13%   - 7
    0.3f   // 15%   - 5.5
};

// Avoid hills "strength". How much do we want to avoid a hill. Combines
// with the usehills factor (1.0 - usehills = avoidhills factor) to create
// a weighting penalty per weighted grade factor. This indicates how strongly
// edges with the specified grade are weighted. Note that speed also is
// influenced by grade, so these weights help further avoid hills.
constexpr float kAvoidHillsStrength[] = {
    2.0f,  // -10%  - Treacherous descent possible
    1.0f,  // -8%   - Steep downhill
    0.5f,  // -6.5% - Good downhill - where is the bottom?
    0.2f,  // -5%   - Picking up speed!
    0.1f,  // -3%   - Modest downhill
    0.0f,  // -1.5% - Smooth slight downhill, ride this all day!
    0.05f, // 0%    - Flat, no avoidance
    0.1f,  // 1.5%  - These are called "false flat"
    0.3f,  // 3%    - Slight rise
    0.8f,  // 5%    - Small hill
    2.0f,  // 6.5%  - Starting to feel this...
    3.0f,  // 8%    - Moderately steep
    4.5f,  // 10%   - Getting tough
    6.5f,  // 11.5% - Tiring!
    10.0f, // 13%   - Ooof - this hurts
    12.0f  // 15%   - Only for the strongest!
};

} // namespace

/**
 * Derived class providing dynamic edge costing for pedestrian routes.
 */
class PedestrianCost : public DynamicCost {
public:
  /**
   * Construct pedestrian costing. Pass in cost type and options using protocol buffer(pbf).
   * @param  costing specified costing type.
   * @param  options pbf with request options.
   */
  PedestrianCost(const Costing costing, const Options& options);

  // virtual destructor
  virtual ~PedestrianCost() {
  }

  /**
   * Does the costing method allow multiple passes (with relaxed hierarchy
   * limits).
   * @return  Returns true if the costing model allows multiple passes.
   */
  virtual bool AllowMultiPass() const {
    return true;
  }

  /**
   * This method overrides the max_distance with the max_distance_mm per segment
   * distance. An example is a pure walking route may have a max distance of
   * 10000 meters (10km) but for a multi-modal route a lower limit of 5000
   * meters per segment (e.g. from origin to a transit stop or from the last
   * transit stop to the destination).
   */
  virtual void UseMaxMultiModalDistance() {
    max_distance_ = transit_start_end_max_distance_;
  }

  /**
   * Returns the maximum transfer distance between stops that you are willing
   * to travel for this mode.  In this case, it is the max walking
   * distance you are willing to walk between transfers.
   */
  virtual uint32_t GetMaxTransferDistanceMM() {
    return transit_transfer_max_distance_;
  }

  /**
   * This method overrides the factor for this mode.  The higher the value
   * the more the mode is favored.
   */
  virtual float GetModeFactor() {
    return mode_factor_;
  }

  /**
   * Get the access mode used by this costing method.
   * @return  Returns access mode.
   */
  uint32_t access_mode() const {
    return access_mask_;
  }

  /**
   * Checks if access is allowed for the provided directed edge.
   * This is generally based on mode of travel and the access modes
   * allowed on the edge. However, it can be extended to exclude access
   * based on other parameters such as conditional restrictions and
   * conditional access that can depend on time and travel mode.
   * @param  edge           Pointer to a directed edge.
   * @param  pred           Predecessor edge information.
   * @param  tile           Current tile.
   * @param  edgeid         GraphId of the directed edge.
   * @param  current_time   Current time (seconds since epoch). A value of 0
   *                        indicates the route is not time dependent.
   * @param  tz_index       timezone index for the node
   * @return Returns true if access is allowed, false if not.
   */
  virtual bool Allowed(const baldr::DirectedEdge* edge,
                       const EdgeLabel& pred,
                       const baldr::GraphTile*& tile,
                       const baldr::GraphId& edgeid,
                       const uint64_t current_time,
                       const uint32_t tz_index,
                       bool& time_restricted) const;

  /**
   * Checks if access is allowed for an edge on the reverse path
   * (from destination towards origin). Both opposing edges (current and
   * predecessor) are provided. The access check is generally based on mode
   * of travel and the access modes allowed on the edge. However, it can be
   * extended to exclude access based on other parameters such as conditional
   * restrictions and conditional access that can depend on time and travel
   * mode.
   * @param  edge           Pointer to a directed edge.
   * @param  pred           Predecessor edge information.
   * @param  opp_edge       Pointer to the opposing directed edge.
   * @param  tile           Current tile.
   * @param  edgeid         GraphId of the opposing edge.
   * @param  current_time   Current time (seconds since epoch). A value of 0
   *                        indicates the route is not time dependent.
   * @param  tz_index       timezone index for the node
   * @return  Returns true if access is allowed, false if not.
   */
  virtual bool AllowedReverse(const baldr::DirectedEdge* edge,
                              const EdgeLabel& pred,
                              const baldr::DirectedEdge* opp_edge,
                              const baldr::GraphTile*& tile,
                              const baldr::GraphId& opp_edgeid,
                              const uint64_t current_time,
                              const uint32_t tz_index,
                              bool& has_time_restrictions) const;

  /**
   * Checks if access is allowed for the provided node. Node access can
   * be restricted if bollards or gates are present.
   * @param  node  Pointer to node information.
   * @return  Returns true if access is allowed, false if not.
   */
  virtual bool Allowed(const baldr::NodeInfo* node) const {
    return (node->access() & access_mask_);
  }

  /**
   * Only transit costings are valid for this method call, hence we throw
   * @param edge
   * @param departure
   * @param curr_time
   * @return
   */
  virtual Cost EdgeCost(const baldr::DirectedEdge* edge,
                        const baldr::TransitDeparture* departure,
                        const uint32_t curr_time) const {
    throw std::runtime_error("PedestrianCost::EdgeCost does not support transit edges");
  }

  /**
   * Get the cost to traverse the specified directed edge. Cost includes
   * the time (seconds) to traverse the edge.
   * @param  edge      Pointer to a directed edge.
   * @param  tile      Current tile.
   * @param  seconds   Time of week in seconds.
   * @return  Returns the cost and time (seconds)
   */
  virtual Cost EdgeCost(const baldr::DirectedEdge* edge,
                        const baldr::GraphTile* tile,
                        const uint32_t seconds) const;

  /**
   * Returns the cost to make the transition from the predecessor edge.
   * Defaults to 0. Costing models that wish to include edge transition
   * costs (i.e., intersection/turn costs) must override this method.
   * @param  edge  Directed edge (the to edge)
   * @param  node  Node (intersection) where transition occurs.
   * @param  pred  Predecessor edge information.
   * @return  Returns the cost and time (seconds)
   */
  virtual Cost TransitionCost(const baldr::DirectedEdge* edge,
                              const baldr::NodeInfo* node,
                              const EdgeLabel& pred) const;

  /**
   * Returns the cost to make the transition from the predecessor edge
   * when using a reverse search (from destination towards the origin).
   * Defaults to 0. Costing models that wish to include edge transition
   * costs (i.e., intersection/turn costs) must override this method.
   * @param  idx   Directed edge local index
   * @param  node  Node (intersection) where transition occurs.
   * @param  pred  the opposing current edge in the reverse tree.
   * @param  edge  the opposing predecessor in the reverse tree
   * @return  Returns the cost and time (seconds)
   */
  virtual Cost TransitionCostReverse(const uint32_t idx,
                                     const baldr::NodeInfo* node,
                                     const baldr::DirectedEdge* pred,
                                     const baldr::DirectedEdge* edge) const;

  /**
   * Get the cost factor for A* heuristics. This factor is multiplied
   * with the distance to the destination to produce an estimate of the
   * minimum cost to the destination. The A* heuristic must underestimate the
   * cost to the destination. So a time based estimate based on speed should
   * assume the maximum speed is used to the destination such that the time
   * estimate is less than the least possible time along roads.
   */
  virtual float AStarCostFactor() const {
    // On first pass use the walking speed plus a small factor to account for
    // favoring walkways, on the second pass use the the maximum ferry speed.
    if (pass_ == 0) {

      // Determine factor based on all of the factor options
      float factor = 1.f;
      if (walkway_factor_ < 1.f) {
        factor *= walkway_factor_;
      }
      if (sidewalk_factor_ < 1.f) {
        factor *= sidewalk_factor_;
      }
      if (alley_factor_ < 1.f) {
        factor *= alley_factor_;
      }
      if (driveway_factor_ < 1.f) {
        factor *= driveway_factor_;
      }

      return (speedfactor_ * factor);
    } else {
      return (kSecPerHour * 0.001f) / static_cast<float>(kMaxFerrySpeedKph);
    }
  }

  /**
   * Get the current travel type.
   * @return  Returns the current travel type.
   */
  virtual uint8_t travel_type() const {
    return static_cast<uint8_t>(type_);
  }

  /**
   * Returns a function/functor to be used in location searching which will
   * exclude and allow ranking results from the search by looking at each
   * edges attribution and suitability for use as a location by the travel
   * mode used by the costing method. Function/functor is also used to filter
   * edges not usable / inaccessible by pedestrians.
   */
  virtual const EdgeFilter GetEdgeFilter() const {
    // Throw back a lambda that checks the access for this type of costing
    auto access_mask = access_mask_;
    auto max_sac_scale = max_hiking_difficulty_;
    return [access_mask, max_sac_scale](const baldr::DirectedEdge* edge) {
      return !(edge->is_shortcut() || edge->use() >= Use::kRail ||
               edge->sac_scale() > max_sac_scale || !(edge->forwardaccess() & access_mask));
    };
  }

  /**
   * Returns a function/functor to be used in location searching which will
   * exclude results from the search by looking at each node's attribution
   * @return Function/functor to be used in filtering out nodes
   */
  virtual const NodeFilter GetNodeFilter() const {
    // throw back a lambda that checks the access for this type of costing
    auto access_mask = access_mask_;
    return [access_mask](const baldr::NodeInfo* node) { return !(node->access() & access_mask); };
  }

public:

  float use_roads_;                     // Preference of using roads between 0 and 1
  float road_factor_;                   // Road factor based on use_roads_
  
  // Type: foot (default), wheelchair, etc.
  PedestrianType type_;

  uint32_t access_mask_;

  // Maximum pedestrian distance.
  uint32_t max_distance_;

  // This is the factor for this mode.  The higher the value the more the
  // mode is favored.
  float mode_factor_;

  // Maximum pedestrian distance in meters for multimodal routes.
  // Maximum distance at the beginning or end of a multimodal route
  // that you are willing to travel for this mode.  In this case,
  // it is the max walking distance.
  uint32_t transit_start_end_max_distance_;

  // Maximum transfer, distance in meters for multimodal routes.
  // Maximum transfer distance between stops that you are willing
  // to travel for this mode.  In this case, it is the max distance
  // you are willing to walk between transfers.
  uint32_t transit_transfer_max_distance_;

  // Minimal surface type usable by the pedestrian type
  Surface minimal_allowed_surface_;

  uint32_t max_grade_;             // Maximum grade (percent).
  SacScale max_hiking_difficulty_; // Max sac_scale (0 - 6)
  float speed_;                    // Pedestrian speed.
  float speedfactor_;              // Speed factor for costing. Based on speed.
  float walkway_factor_;           // Factor for favoring walkways and paths.
  float sidewalk_factor_;          // Factor for favoring sidewalks.
  float alley_factor_;             // Avoid alleys factor.
  float driveway_factor_;          // Avoid driveways factor.
  float step_penalty_;             // Penalty applied to steps/stairs (seconds).

  // Elevation/grade penalty (weighting applied based on the edge's weighted
  // grade (relative value from 0-15)
  float grade_penalty[16];

  /**
   * Override the base transition cost to not add maneuver penalties onto transit edges.
   * Base transition cost that all costing methods use. Includes costs for
   * country crossing, boarding a ferry, toll booth, gates, entering destination
   * only, alleys, and maneuver penalties. Each costing method can provide different
   * costs for these transitions (via costing options).
   * @param node Node at the intersection where the edge transition occurs.
   * @param edge Directed edge entering.
   * @param pred Predecessor edge information.
   * @param idx  Index used for name consistency.
   * @return Returns the transition cost (cost, elapsed time).
   */
  sif::Cost base_transition_cost(const baldr::NodeInfo* node,
                                 const baldr::DirectedEdge* edge,
                                 const sif::EdgeLabel& pred,
                                 const uint32_t idx) const {
    // Cases with both time and penalty: country crossing, ferry, gate, toll booth
    sif::Cost c;
    if (node->type() == baldr::NodeType::kBorderControl) {
      c += country_crossing_cost_;
    }
    if (node->type() == baldr::NodeType::kGate) {
      c += gate_cost_;
    }
    if (node->type() == baldr::NodeType::kTollBooth) {
      c += toll_booth_cost_;
    }
    if (edge->use() == baldr::Use::kFerry && pred.use() != baldr::Use::kFerry) {
      c += ferry_transition_cost_;
    }

    // Additional penalties without any time cost
    if (edge->destonly() && !pred.destonly()) {
      c.cost += destination_only_penalty_;
    }
    if (edge->use() == baldr::Use::kAlley && pred.use() != baldr::Use::kAlley) {
      c.cost += alley_penalty_;
    }
    if (!edge->link() && edge->use() != Use::kEgressConnection &&
        edge->use() != Use::kPlatformConnection && !edge->name_consistency(idx)) {
      c.cost += maneuver_penalty_;
    }
    return c;
  }

  /**
   * Override the base transition cost to not add maneuver penalties onto transit edges.
   * Base transition cost that all costing methods use. Includes costs for
   * country crossing, boarding a ferry, toll booth, gates, entering destination
   * only, alleys, and maneuver penalties. Each costing method can provide different
   * costs for these transitions (via costing options).
   * @param node Node at the intersection where the edge transition occurs.
   * @param edge Directed edge entering.
   * @param pred Predecessor edge.
   * @param idx  Index used for name consistency.
   * @return Returns the transition cost (cost, elapsed time).
   */
  sif::Cost base_transition_cost(const baldr::NodeInfo* node,
                                 const baldr::DirectedEdge* edge,
                                 const baldr::DirectedEdge* pred,
                                 const uint32_t idx) const {
    // Cases with both time and penalty: country crossing, ferry, gate, toll booth
    sif::Cost c;
    if (node->type() == baldr::NodeType::kBorderControl) {
      c += country_crossing_cost_;
    }
    if (node->type() == baldr::NodeType::kGate) {
      c += gate_cost_;
    }
    if (node->type() == baldr::NodeType::kTollBooth) {
      c += toll_booth_cost_;
    }
    if (edge->use() == baldr::Use::kFerry && pred->use() != baldr::Use::kFerry) {
      c += ferry_transition_cost_;
    }

    // Additional penalties without any time cost
    if (edge->destonly() && !pred->destonly()) {
      c.cost += destination_only_penalty_;
    }
    if (edge->use() == baldr::Use::kAlley && pred->use() != baldr::Use::kAlley) {
      c.cost += alley_penalty_;
    }
    if (!edge->link() && edge->use() != Use::kEgressConnection &&
        edge->use() != Use::kPlatformConnection && !edge->name_consistency(idx)) {
      c.cost += maneuver_penalty_;
    }
    return c;
  }
};

// Constructor. Parse pedestrian options from property tree. If option is
// not present, set the default.
PedestrianCost::PedestrianCost(const Costing costing, const Options& options)
    : DynamicCost(options, TravelMode::kPedestrian) {
  // Grab the costing options based on the specified costing type
  const CostingOptions& costing_options = options.costing_options(static_cast<int>(costing));

  // Set hierarchy to allow unlimited transitions
  for (auto& h : hierarchy_limits_) {
    h.max_up_transitions = kUnlimitedTransitions;
  }

  allow_transit_connections_ = false;

  // Get the base costs
  get_base_costs(costing_options);

  // Get the pedestrian type - enter as string and convert to enum
  std::string type = costing_options.transport_type();
  if (type == "wheelchair") {
    type_ = PedestrianType::kWheelchair;
  } else if (type == "segway") {
    type_ = PedestrianType::kSegway;
  } else {
    type_ = PedestrianType::kFoot;
  }

  // Set type specific defaults, override with URL inputs
  if (type_ == PedestrianType::kWheelchair) {
    access_mask_ = kWheelchairAccess;
    minimal_allowed_surface_ = Surface::kCompacted;
  } else {
    // Assume type = foot
    access_mask_ = kPedestrianAccess;
    minimal_allowed_surface_ = Surface::kPath;
  }
  max_distance_ = costing_options.max_distance();
  speed_ = costing_options.walking_speed();
  step_penalty_ = costing_options.step_penalty();
  max_grade_ = costing_options.max_grade();

  if (type_ == PedestrianType::kFoot) {
    max_hiking_difficulty_ = static_cast<SacScale>(costing_options.max_hiking_difficulty());
  } else {
    max_hiking_difficulty_ = SacScale::kNone;
  }

  // Willingness to use roads. Make sure this is within range [0, 1].
  use_roads_ = costing_options.use_roads();

  // Set the road classification factor. use_roads factors above 0.5 start to
  // reduce the weight difference between road classes while factors below 0.5
  // start to increase the differences.
  road_factor_ = (use_roads_ >= 0.5f) ? 1.5f - use_roads_ : 2.0f - use_roads_ * 2.0f;

  mode_factor_ = costing_options.mode_factor();
  walkway_factor_ = costing_options.walkway_factor();
  sidewalk_factor_ = costing_options.sidewalk_factor();
  alley_factor_ = costing_options.alley_factor();
  driveway_factor_ = costing_options.driveway_factor();
  transit_start_end_max_distance_ = costing_options.transit_start_end_max_distance();
  transit_transfer_max_distance_ = costing_options.transit_transfer_max_distance();

  // Set the speed factor (to avoid division in costing)
  speedfactor_ = (kSecPerHour * 0.001f) / speed_;


  // Populate the grade penalties (based on use_hills factor - value between 0 and 1)
  float use_hills = costing_options.use_hills();
  float avoid_hills = (1.0f - use_hills);
  for (uint32_t i = 0; i <= kMaxGradeFactor; i++) {
    grade_penalty[i] = avoid_hills * kAvoidHillsStrength[i];
  }
}

// Check if access is allowed on the specified edge. Disallow if no
// access for this pedestrian type, if surface type exceeds (worse than)
// the minimum allowed surface type, or if max grade is exceeded.
// Disallow edges where max. distance will be exceeded.
bool PedestrianCost::Allowed(const baldr::DirectedEdge* edge,
                             const EdgeLabel& pred,
                             const baldr::GraphTile*& tile,
                             const baldr::GraphId& edgeid,
                             const uint64_t current_time,
                             const uint32_t tz_index,
                             bool& has_time_restrictions) const {
  if (!(edge->forwardaccess() & access_mask_) || (edge->surface() > minimal_allowed_surface_) ||
      edge->is_shortcut() || IsUserAvoidEdge(edgeid) || edge->sac_scale() > max_hiking_difficulty_ ||
      //      (edge->max_up_slope() > max_grade_ || edge->max_down_slope() > max_grade_) ||
      ((pred.path_distance() + edge->length()) > max_distance_)) {
    return false;
  }
  // Disallow transit connections (except when set for multi-modal routes)
  if (!allow_transit_connections_ &&
      (edge->use() == Use::kPlatformConnection || edge->use() == Use::kEgressConnection ||
       edge->use() == Use::kTransitConnection)) {
    return false;
  }

  return DynamicCost::EvaluateRestrictions(access_mask_, edge, tile, edgeid, current_time, tz_index,
                                           has_time_restrictions);
}

// Checks if access is allowed for an edge on the reverse path (from
// destination towards origin). Both opposing edges are provided.
bool PedestrianCost::AllowedReverse(const baldr::DirectedEdge* edge,
                                    const EdgeLabel& pred,
                                    const baldr::DirectedEdge* opp_edge,
                                    const baldr::GraphTile*& tile,
                                    const baldr::GraphId& opp_edgeid,
                                    const uint64_t current_time,
                                    const uint32_t tz_index,
                                    bool& has_time_restrictions) const {
  // TODO - obtain and check the access restrictions.

  // Do not check max walking distance and assume we are not allowing
  // transit connections. Assume this method is never used in
  // multimodal routes).
  if (!(opp_edge->forwardaccess() & access_mask_) ||
      (opp_edge->surface() > minimal_allowed_surface_) || opp_edge->is_shortcut() ||
      IsUserAvoidEdge(opp_edgeid) || edge->sac_scale() > max_hiking_difficulty_ ||
      //      (opp_edge->max_up_slope() > max_grade_ || opp_edge->max_down_slope() > max_grade_) ||
      opp_edge->use() == Use::kTransitConnection || opp_edge->use() == Use::kEgressConnection ||
      opp_edge->use() == Use::kPlatformConnection) {
    return false;
  }

  return DynamicCost::EvaluateRestrictions(access_mask_, edge, tile, opp_edgeid, current_time,
                                           tz_index, has_time_restrictions);
}

// Returns the cost to traverse the edge and an estimate of the actual time
// (in seconds) to traverse the edge.
Cost PedestrianCost::EdgeCost(const baldr::DirectedEdge* edge,
                              const baldr::GraphTile* tile,
                              const uint32_t seconds) const {

  // Ferries are a special case - they use the ferry speed (stored on the edge)
  if (edge->use() == Use::kFerry) {
    auto speed = tile->GetSpeed(edge, flow_mask_, seconds);
    float sec = edge->length() * (kSecPerHour * 0.001f) / static_cast<float>(speed);
    return {sec * ferry_factor_, sec};
  }

  // Represents how stressful a roadway is without looking at grade or cycle accommodations
  float roadway_stress = 1.0f;

  float accommodation_factor = 1.0f;

  // Special use cases: cycleway, footway, and path
  if (edge->use() == Use::kCycleway || edge->use() == Use::kFootway || edge->use() == Use::kPath) {

    // Differentiate how segregated the way is from pedestrians
    if (edge->cyclelane() == CycleLane::kSeparated) { // No pedestrians allowed on path
      accommodation_factor = use_roads_ * 0.8f;
    } else if (edge->cyclelane() == CycleLane::kDedicated) { // Segregated lane from pedestrians
      accommodation_factor = 0.1f + use_roads_ * 0.9f;
    } else { // Share path with pedestrians
      accommodation_factor = 0.2f + use_roads_;
    }
  } else if (edge->use() == Use::kMountainBike) {
    // Slightly less reduction than a footway or path because even with a mountain bike
    // these paths can be a little stressful to ride. No traffic though so still favorable
    accommodation_factor = 0.3f + use_roads_;
  } else if (edge->use() == Use::kLivingStreet) {
    roadway_stress = 0.2f + use_roads_ * 0.8f;
  } else if (edge->use() == Use::kTrack) {
    roadway_stress = 0.5f + use_roads_;
  } else {
    // Favor roads where a cycle lane exists
    if (edge->cyclelane() == CycleLane::kShared) {
      accommodation_factor = 0.9f + use_roads_ * 0.05f;
    } else if (edge->cyclelane() == CycleLane::kDedicated) {
      accommodation_factor = 0.4f + use_roads_ * 0.45f;
    } else if (edge->cyclelane() == CycleLane::kSeparated) {
      accommodation_factor = 0.15f + use_roads_ * 0.6f;
    } else if (edge->shoulder()) {
      // If no cycle lane, but there is a shoulder then have a slight preference for this road
      accommodation_factor = 0.7f + use_roads_ * 0.2f;
    }

    // Penalize roads that have more than one lane (in the direction of travel)
    if (edge->lanecount() > 1) {
      roadway_stress += (static_cast<float>(edge->lanecount()) - 1) * 0.05f * road_factor_;
    }

    // Add in penalization for road classification
    roadway_stress += road_factor_ * kRoadClassFactor[static_cast<uint32_t>(edge->classification())];
  }
  
  // Penalize roads that have more than one lane (in the direction of travel)
  // Add in penalization for road classification
  roadway_stress += road_factor_ * kRoadClassFactor[static_cast<uint32_t>(edge->classification())];

  float total_stress = accommodation_factor * roadway_stress;

  // TODO - consider using an array of "use factors" to avoid this conditional
  float factor = 1.0f + kSacScaleCostFactor[static_cast<uint8_t>(edge->sac_scale())] + grade_penalty[edge->weighted_grade()] + total_stress;
  if (edge->use() == Use::kFootway || edge->use() == Use::kSidewalk) {
    factor *= walkway_factor_;
  } else if (edge->use() == Use::kAlley) {
    factor *= alley_factor_;
  } else if (edge->use() == Use::kDriveway) {
    factor *= driveway_factor_;
  } else if (edge->sidewalk_left() || edge->sidewalk_right()) {
    factor *= sidewalk_factor_;
  } else if (edge->roundabout()) {
    factor *= kRoundaboutFactor;
  }

  // Slightly favor walkways/paths and penalize alleys and driveways.
  float sec =
      edge->length() * speedfactor_ * kSacScaleSpeedFactor[static_cast<uint8_t>(edge->sac_scale())];
  return {sec * factor, sec};
}

// Returns the time (in seconds) to make the transition from the predecessor
Cost PedestrianCost::TransitionCost(const baldr::DirectedEdge* edge,
                                    const baldr::NodeInfo* node,
                                    const EdgeLabel& pred) const {
  // Special cases: fixed penalty for steps/stairs
  if (edge->use() == Use::kSteps) {
    return {step_penalty_, 0.0f};
  }

  // Get the transition cost for country crossing, ferry, gate, toll booth,
  // destination only, alley, maneuver penalty
  uint32_t idx = pred.opp_local_idx();
  Cost c = base_transition_cost(node, edge, pred, idx);

  // Costs for crossing an intersection.
  if (edge->edge_to_right(idx) && edge->edge_to_left(idx)) {
    float seconds = kCrossingCosts[edge->stopimpact(idx)];
    c.secs += seconds;
    c.cost += seconds;
  }
  return c;
}

// Returns the cost to make the transition from the predecessor edge
// when using a reverse search (from destination towards the origin).
// Defaults to 0. Costing models that wish to include edge transition
// costs (i.e., intersection/turn costs) must override this method.
Cost PedestrianCost::TransitionCostReverse(const uint32_t idx,
                                           const baldr::NodeInfo* node,
                                           const baldr::DirectedEdge* pred,
                                           const baldr::DirectedEdge* edge) const {
  // Special cases: fixed penalty for steps/stairs
  if (edge->use() == Use::kSteps) {
    return {step_penalty_, 0.0f};
  }

  // Get the transition cost for country crossing, ferry, gate, toll booth,
  // destination only, alley, maneuver penalty
  Cost c = base_transition_cost(node, edge, pred, idx);

  // Costs for crossing an intersection.
  if (edge->edge_to_right(idx) && edge->edge_to_left(idx)) {
    float seconds = kCrossingCosts[edge->stopimpact(idx)];
    c.secs += seconds;
    c.cost += seconds;
  }
  return c;
}

void ParsePedestrianCostOptions(const rapidjson::Document& doc,
                                const std::string& costing_options_key,
                                CostingOptions* pbf_costing_options) {
  auto json_costing_options = rapidjson::get_child_optional(doc, costing_options_key.c_str());

  if (json_costing_options) {
    // TODO: farm more common stuff out to parent class
    ParseCostOptions(*json_costing_options, pbf_costing_options);

    // If specified, parse json and set pbf values

    // maneuver_penalty
    pbf_costing_options->set_maneuver_penalty(kManeuverPenaltyRange(
        rapidjson::get_optional<float>(*json_costing_options, "/maneuver_penalty")
            .get_value_or(kDefaultManeuverPenalty)));

    // destination_only_penalty
    pbf_costing_options->set_destination_only_penalty(kDestinationOnlyPenaltyRange(
        rapidjson::get_optional<float>(*json_costing_options, "/destination_only_penalty")
            .get_value_or(kDefaultDestinationOnlyPenalty)));

    // gate_penalty
    pbf_costing_options->set_gate_penalty(
        kGatePenaltyRange(rapidjson::get_optional<float>(*json_costing_options, "/gate_penalty")
                              .get_value_or(kDefaultGatePenalty)));

    // alley_penalty
    pbf_costing_options->set_alley_penalty(
        kAlleyPenaltyRange(rapidjson::get_optional<float>(*json_costing_options, "/alley_penalty")
                               .get_value_or(kDefaultAlleyPenalty)));

    // country_crossing_cost
    pbf_costing_options->set_country_crossing_cost(kCountryCrossingCostRange(
        rapidjson::get_optional<float>(*json_costing_options, "/country_crossing_cost")
            .get_value_or(kDefaultCountryCrossingCost)));

    // country_crossing_penalty
    pbf_costing_options->set_country_crossing_penalty(kCountryCrossingPenaltyRange(
        rapidjson::get_optional<float>(*json_costing_options, "/country_crossing_penalty")
            .get_value_or(kDefaultCountryCrossingPenalty)));

    // use_roads
    pbf_costing_options->set_use_roads(
        kUseRoadRange(rapidjson::get_optional<float>(*json_costing_options, "/use_roads")
                          .get_value_or(kDefaultUseRoad)));

    // use_hills
    pbf_costing_options->set_use_hills(
        kUseHillsRange(rapidjson::get_optional<float>(*json_costing_options, "/use_hills")
                           .get_value_or(kDefaultUseHills)));

    // ferry_cost
    pbf_costing_options->set_ferry_cost(
        kFerryCostRange(rapidjson::get_optional<float>(*json_costing_options, "/ferry_cost")
                            .get_value_or(kDefaultFerryCost)));

    // use_ferry
    pbf_costing_options->set_use_ferry(
        kUseFerryRange(rapidjson::get_optional<float>(*json_costing_options, "/use_ferry")
                           .get_value_or(kDefaultUseFerry)));

    // type (transport_type)
    pbf_costing_options->set_transport_type(
        (rapidjson::get_optional<std::string>(*json_costing_options, "/type").get_value_or("foot")));

    // Set type specific defaults, override with URL inputs
    if (pbf_costing_options->transport_type() == "wheelchair") {
      // max_distance
      pbf_costing_options->set_max_distance(kMaxDistanceWheelchairRange(
          rapidjson::get_optional<uint32_t>(*json_costing_options, "/max_distance")
              .get_value_or(kMaxDistanceWheelchair)));

      // walking_speed
      pbf_costing_options->set_walking_speed(kSpeedWheelchairRange(
          rapidjson::get_optional<float>(*json_costing_options, "/walking_speed")
              .get_value_or(kDefaultSpeedWheelchair)));

      // step_penalty
      pbf_costing_options->set_step_penalty(kStepPenaltyWheelchairRange(
          rapidjson::get_optional<float>(*json_costing_options, "/step_penalty")
              .get_value_or(kDefaultStepPenaltyWheelchair)));

      // max_grade
      pbf_costing_options->set_max_grade(kMaxGradeWheelchairRange(
          rapidjson::get_optional<uint32_t>(*json_costing_options, "/max_grade")
              .get_value_or(kDefaultMaxGradeWheelchair)));

    } else {
      // Assume type = foot
      // max_distance
      pbf_costing_options->set_max_distance(kMaxDistanceFootRange(
          rapidjson::get_optional<uint32_t>(*json_costing_options, "/max_distance")
              .get_value_or(kMaxDistanceFoot)));

      // walking_speed
      pbf_costing_options->set_walking_speed(
          kSpeedFootRange(rapidjson::get_optional<float>(*json_costing_options, "/walking_speed")
                              .get_value_or(kDefaultSpeedFoot)));

      // step_penalty
      pbf_costing_options->set_step_penalty(
          kStepPenaltyFootRange(rapidjson::get_optional<float>(*json_costing_options, "/step_penalty")
                                    .get_value_or(kDefaultStepPenaltyFoot)));

      // max_grade
      pbf_costing_options->set_max_grade(
          kMaxGradeFootRange(rapidjson::get_optional<uint32_t>(*json_costing_options, "/max_grade")
                                 .get_value_or(kDefaultMaxGradeFoot)));
    }

    // max_hiking_difficulty
    pbf_costing_options->set_max_hiking_difficulty(kMaxHikingDifficultyRange(
        rapidjson::get_optional<uint32_t>(*json_costing_options, "/max_hiking_difficulty")
            .get_value_or(kDefaultMaxHikingDifficulty)));

    // mode_factor
    pbf_costing_options->set_mode_factor(
        kModeFactorRange(rapidjson::get_optional<float>(*json_costing_options, "/mode_factor")
                             .get_value_or(kModeFactor)));

    // walkway_factor
    pbf_costing_options->set_walkway_factor(
        kWalkwayFactorRange(rapidjson::get_optional<float>(*json_costing_options, "/walkway_factor")
                                .get_value_or(kDefaultWalkwayFactor)));

    // sidewalk_factor
    pbf_costing_options->set_sidewalk_factor(
        kSideWalkFactorRange(rapidjson::get_optional<float>(*json_costing_options, "/sidewalk_factor")
                                 .get_value_or(kDefaultSideWalkFactor)));

    // alley_factor
    pbf_costing_options->set_alley_factor(
        kAlleyFactorRange(rapidjson::get_optional<float>(*json_costing_options, "/alley_factor")
                              .get_value_or(kDefaultAlleyFactor)));

    // driveway_factor
    pbf_costing_options->set_driveway_factor(
        kDrivewayFactorRange(rapidjson::get_optional<float>(*json_costing_options, "/driveway_factor")
                                 .get_value_or(kDefaultDrivewayFactor)));

    // transit_start_end_max_distance
    pbf_costing_options->set_transit_start_end_max_distance(kTransitStartEndMaxDistanceRange(
        rapidjson::get_optional<uint32_t>(*json_costing_options, "/transit_start_end_max_distance")
            .get_value_or(kTransitStartEndMaxDistance)));

    // transit_transfer_max_distance
    pbf_costing_options->set_transit_transfer_max_distance(kTransitTransferMaxDistanceRange(
        rapidjson::get_optional<uint32_t>(*json_costing_options, "/transit_transfer_max_distance")
            .get_value_or(kTransitTransferMaxDistance)));

  } else {
    // Set pbf values to defaults
    pbf_costing_options->set_maneuver_penalty(kDefaultManeuverPenalty);
    pbf_costing_options->set_destination_only_penalty(kDefaultDestinationOnlyPenalty);
    pbf_costing_options->set_gate_penalty(kDefaultGatePenalty);
    pbf_costing_options->set_alley_penalty(kDefaultAlleyPenalty);
    pbf_costing_options->set_country_crossing_cost(kDefaultCountryCrossingCost);
    pbf_costing_options->set_country_crossing_penalty(kDefaultCountryCrossingPenalty);
    pbf_costing_options->set_ferry_cost(kDefaultFerryCost);
    pbf_costing_options->set_use_ferry(kDefaultUseFerry);
    pbf_costing_options->set_use_roads(kDefaultUseRoad);
    pbf_costing_options->set_use_hills(kDefaultUseHills);
    pbf_costing_options->set_transport_type("foot");
    pbf_costing_options->set_max_distance(kMaxDistanceFoot);
    pbf_costing_options->set_walking_speed(kDefaultSpeedFoot);
    pbf_costing_options->set_step_penalty(kDefaultStepPenaltyFoot);
    pbf_costing_options->set_max_grade(kDefaultMaxGradeFoot);
    pbf_costing_options->set_max_hiking_difficulty(kDefaultMaxHikingDifficulty);
    pbf_costing_options->set_mode_factor(kModeFactor);
    pbf_costing_options->set_walkway_factor(kDefaultWalkwayFactor);
    pbf_costing_options->set_sidewalk_factor(kDefaultSideWalkFactor);
    pbf_costing_options->set_alley_factor(kDefaultAlleyFactor);
    pbf_costing_options->set_driveway_factor(kDefaultDrivewayFactor);
    pbf_costing_options->set_transit_start_end_max_distance(kTransitStartEndMaxDistance);
    pbf_costing_options->set_transit_transfer_max_distance(kTransitTransferMaxDistance);
  }
}

cost_ptr_t CreatePedestrianCost(const Costing costing, const Options& options) {
  return std::make_shared<PedestrianCost>(costing, options);
}

} // namespace sif
} // namespace valhalla

/**********************************************************************************************/

#ifdef INLINE_TEST

using namespace valhalla;
using namespace sif;

namespace {

class TestPedestrianCost : public PedestrianCost {
public:
  TestPedestrianCost(const Costing costing, const Options& options)
      : PedestrianCost(costing, options){};

  using PedestrianCost::alley_penalty_;
  using PedestrianCost::country_crossing_cost_;
  using PedestrianCost::destination_only_penalty_;
  using PedestrianCost::ferry_transition_cost_;
  using PedestrianCost::gate_cost_;
  using PedestrianCost::maneuver_penalty_;
};

TestPedestrianCost*
make_pedestriancost_from_json(const std::string& property, float testVal, const std::string& type) {
  std::stringstream ss;
  ss << R"({"costing_options":{"pedestrian":{")" << property << R"(":)" << testVal << "}}}";
  Api request;
  ParseApi(ss.str(), valhalla::Options::route, request);
  return new TestPedestrianCost(valhalla::Costing::pedestrian, request.options());
}

std::uniform_real_distribution<float>*
make_distributor_from_range(const ranged_default_t<float>& range) {
  float rangeLength = range.max - range.min;
  return new std::uniform_real_distribution<float>(range.min - rangeLength, range.max + rangeLength);
}

std::uniform_int_distribution<uint32_t>*
make_distributor_from_range(const ranged_default_t<uint32_t>& range) {
  uint32_t rangeLength = range.max - range.min;
  return new std::uniform_int_distribution<uint32_t>(range.min - rangeLength,
                                                     range.max + rangeLength);
}

void testPedestrianCostParams() {
  constexpr unsigned testIterations = 250;
  constexpr unsigned seed = 0;
  std::mt19937 generator(seed);
  std::shared_ptr<std::uniform_real_distribution<float>> real_distributor;
  std::shared_ptr<std::uniform_int_distribution<uint32_t>> int_distributor;
  std::shared_ptr<TestPedestrianCost> ctorTester;

  // maneuver_penalty_
  real_distributor.reset(make_distributor_from_range(kManeuverPenaltyRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("maneuver_penalty", (*real_distributor)(generator), "foot"));
    if (ctorTester->maneuver_penalty_ < kManeuverPenaltyRange.min ||
        ctorTester->maneuver_penalty_ > kManeuverPenaltyRange.max) {
      throw std::runtime_error("maneuver_penalty_ is not within it's range");
    }
  }

  // gate_penalty_
  real_distributor.reset(make_distributor_from_range(kGatePenaltyRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("gate_penalty", (*real_distributor)(generator), "foot"));
    if (ctorTester->gate_cost_.cost < kGatePenaltyRange.min ||
        ctorTester->gate_cost_.cost > kGatePenaltyRange.max) {
      throw std::runtime_error("gate_penalty_ is not within it's range");
    }
  }

  // alley_factor_
  real_distributor.reset(make_distributor_from_range(kAlleyFactorRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("alley_factor", (*real_distributor)(generator), "foot"));
    if (ctorTester->alley_factor_ < kAlleyFactorRange.min ||
        ctorTester->alley_factor_ > kAlleyFactorRange.max) {
      throw std::runtime_error("alley_factor_ is not within it's range");
    }
  }

  // ferry_cost_
  real_distributor.reset(make_distributor_from_range(kFerryCostRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("ferry_cost", (*real_distributor)(generator), "foot"));
    if (ctorTester->ferry_transition_cost_.secs < kFerryCostRange.min ||
        ctorTester->ferry_transition_cost_.secs > kFerryCostRange.max) {
      throw std::runtime_error("ferry_cost_ is not within it's range");
    }
  }

  // country_crossing_cost_
  real_distributor.reset(make_distributor_from_range(kCountryCrossingCostRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(make_pedestriancost_from_json("country_crossing_cost",
                                                   (*real_distributor)(generator), "foot"));
    if (ctorTester->country_crossing_cost_.secs < kCountryCrossingCostRange.min ||
        ctorTester->country_crossing_cost_.secs > kCountryCrossingCostRange.max) {
      throw std::runtime_error("country_crossing_cost_ is not within it's range");
    }
  }

  // country_crossing_penalty_
  real_distributor.reset(make_distributor_from_range(kCountryCrossingPenaltyRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(make_pedestriancost_from_json("country_crossing_penalty",
                                                   (*real_distributor)(generator), "foot"));
    if (ctorTester->country_crossing_cost_.cost < kCountryCrossingPenaltyRange.min ||
        ctorTester->country_crossing_cost_.cost >
            kCountryCrossingPenaltyRange.max + kDefaultCountryCrossingCost) {
      throw std::runtime_error("country_crossing_penalty_ is not within it's range");
    }
  }

  // Wheelchair tests
  // max_distance_
  int_distributor.reset(make_distributor_from_range(kMaxDistanceWheelchairRange));
  for (unsigned i = 0; i < 100; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("max_distance", (*int_distributor)(generator), "wheelchair"));
    if (ctorTester->max_distance_ < kMaxDistanceWheelchairRange.min ||
        ctorTester->max_distance_ > kMaxDistanceWheelchairRange.max) {
      throw std::runtime_error("max_distance_ with type wheelchair is not within it's range");
    }
  }

  // speed_
  real_distributor.reset(make_distributor_from_range(kSpeedWheelchairRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("walking_speed", (*real_distributor)(generator), "wheelchair"));
    if (ctorTester->speed_ < kSpeedWheelchairRange.min ||
        ctorTester->speed_ > kSpeedWheelchairRange.max) {
      throw std::runtime_error("speed_ with type wheelchair is not within it's range");
    }
  }

  // step_penalty_
  real_distributor.reset(make_distributor_from_range(kStepPenaltyWheelchairRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("step_penalty", (*real_distributor)(generator), "wheelchair"));
    if (ctorTester->step_penalty_ < kStepPenaltyWheelchairRange.min ||
        ctorTester->step_penalty_ > kStepPenaltyWheelchairRange.max) {
      throw std::runtime_error("step_penalty_ with type wheelchair is not within it's range");
    }
  }

  // max_grade_
  int_distributor.reset(make_distributor_from_range(kMaxGradeWheelchairRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("max_grade", (*int_distributor)(generator), "wheelchair"));
    if (ctorTester->max_grade_ < kMaxGradeWheelchairRange.min ||
        ctorTester->max_grade_ > kMaxGradeWheelchairRange.max) {
      throw std::runtime_error("max_grade_ with type wheelchair is not within it's range");
    }
  }

  // Foot tests
  // max_distance_
  int_distributor.reset(make_distributor_from_range(kMaxDistanceFootRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("max_distance", (*int_distributor)(generator), "foot"));
    if (ctorTester->max_distance_ < kMaxDistanceFootRange.min ||
        ctorTester->max_distance_ > kMaxDistanceFootRange.max) {
      throw std::runtime_error("max_distance_ with type foot is not within it's range");
    }
  }

  // speed_
  real_distributor.reset(make_distributor_from_range(kSpeedFootRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("walking_speed", (*real_distributor)(generator), "foot"));
    if (ctorTester->speed_ < kSpeedFootRange.min || ctorTester->speed_ > kSpeedFootRange.max) {
      throw std::runtime_error("speed_ with type foot is not within it's range");
    }
  }

  // step_penalty_
  real_distributor.reset(make_distributor_from_range(kStepPenaltyFootRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("step_penalty", (*real_distributor)(generator), "foot"));
    if (ctorTester->step_penalty_ < kStepPenaltyFootRange.min ||
        ctorTester->step_penalty_ > kStepPenaltyFootRange.max) {
      throw std::runtime_error("step_penalty_ with type foot is not within it's range");
    }
  }

  // max_grade_
  int_distributor.reset(make_distributor_from_range(kMaxGradeFootRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("max_grade", (*int_distributor)(generator), "foot"));
    if (ctorTester->max_grade_ < kMaxGradeFootRange.min ||
        ctorTester->max_grade_ > kMaxGradeFootRange.max) {
      throw std::runtime_error("max_grade_ with type foot is not within it's range");
    }
  }

  // Non type dependent tests
  // mode_factor_
  real_distributor.reset(make_distributor_from_range(kModeFactorRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("mode_factor", (*real_distributor)(generator), "foot"));
    if (ctorTester->mode_factor_ < kModeFactorRange.min ||
        ctorTester->mode_factor_ > kModeFactorRange.max) {
      throw std::runtime_error("mode_factor_ is not within it's range");
    }
  }

  /*
   // use_ferry_
   real_distributor.reset(make_distributor_from_range(kUseFerryRange));
   for (unsigned i = 0; i < testIterations; ++i) {
     ctorTester.reset(
         make_pedestriancost_from_json("use_ferry", (*real_distributor)(generator), "foot"));
     if (ctorTester->use_ferry_ < kUseFerryRange.min || ctorTester->use_ferry_ > kUseFerryRange.max) {
       throw std::runtime_error("use_ferry_ is not within it's range");
     }
   }

   */

  // walkway_factor_
  real_distributor.reset(make_distributor_from_range(kWalkwayFactorRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("walkway_factor", (*real_distributor)(generator), "foot"));
    if (ctorTester->walkway_factor_ < kWalkwayFactorRange.min ||
        ctorTester->walkway_factor_ > kWalkwayFactorRange.max) {
      throw std::runtime_error("walkway_factor_ is not within it's range");
    }
  }

  // sidewalk_factor_
  real_distributor.reset(make_distributor_from_range(kSideWalkFactorRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("sidewalk_factor", (*real_distributor)(generator), "foot"));
    if (ctorTester->sidewalk_factor_ < kSideWalkFactorRange.min ||
        ctorTester->sidewalk_factor_ > kSideWalkFactorRange.max) {
      throw std::runtime_error("sidewalk_factor_ is not within it's range");
    }
  }

  // driveway_factor_
  real_distributor.reset(make_distributor_from_range(kDrivewayFactorRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(
        make_pedestriancost_from_json("driveway_factor", (*real_distributor)(generator), "foot"));
    if (ctorTester->driveway_factor_ < kDrivewayFactorRange.min ||
        ctorTester->driveway_factor_ > kDrivewayFactorRange.max) {
      throw std::runtime_error("driveway_factor_ is not within it's range");
    }
  }

  // transit_start_end_max_distance_
  int_distributor.reset(make_distributor_from_range(kTransitStartEndMaxDistanceRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(make_pedestriancost_from_json("transit_start_end_max_distance",
                                                   (*int_distributor)(generator), "foot"));
    if (ctorTester->transit_start_end_max_distance_ < kTransitStartEndMaxDistanceRange.min ||
        ctorTester->transit_start_end_max_distance_ > kTransitStartEndMaxDistanceRange.max) {
      throw std::runtime_error("transit_start_end_max_distance_ is not within it's range");
    }
  }

  // transit_transfer_max_distance_
  int_distributor.reset(make_distributor_from_range(kTransitTransferMaxDistanceRange));
  for (unsigned i = 0; i < testIterations; ++i) {
    ctorTester.reset(make_pedestriancost_from_json("transit_transfer_max_distance",
                                                   (*int_distributor)(generator), "foot"));
    if (ctorTester->transit_transfer_max_distance_ < kTransitTransferMaxDistanceRange.min ||
        ctorTester->transit_transfer_max_distance_ > kTransitTransferMaxDistanceRange.max) {
      throw std::runtime_error("transit_transfer_max_distance_ is not within it's range");
    }
  }
}
} // namespace

int main() {
  test::suite suite("costing");

  suite.test(TEST_CASE(testPedestrianCostParams));

  return suite.tear_down();
}

#endif
