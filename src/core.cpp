/**
 * @file core.cpp
 * @brief Game-based implementation of Relaxed Weak Timed Bisimulation (RTWBS).
 *
 * This file contains a lightweight on-the-fly game algorithm for checking
 * relaxed weak timed bisimulation between timed automata. The notion of
 * relaxation follows the asymmetric timing constraints:
 *  - Sent actions (!) in the refined automaton must be at least as restrictive
 *    (i.e., enabling zone is a subset) as in the abstract one.
 *  - Received actions (?) in the refined automaton may be more permissive
 *    (i.e., enabling zone is a superset) than in the abstract one.
 *  - Internal (unsynchronized / tau) actions are matched under standard
 *    weak semantics (tau* a tau*), requiring refined enabling ⊆ abstract.
 *
 * The algorithm keeps a candidate relation R of zone pairs (location + DBM).
 * It eliminates violating pairs until reaching a greatest fixed point.
 * If the relation is non-empty after convergence, refinement holds.
 *
 * NOTE: This is a first clean version focused on correctness/clarity.
 * Future optimisations: memoisation of tau-closure, weak successors,
 * on-demand zone graph expansion, and predecessor dependency tracking.
 */
// Clean reimplementation (game-based RTWBS)
#include "rtwbs/core.h"
#include <chrono>
#include <queue>
#include <unordered_set>
#include <iostream>

namespace rtwbs {

namespace { // anonymous helpers (not part of public API)
/**
 * @brief Hash functor for pairs of zone state pointers.
 * @details Used to store candidate relation pairs in an unordered_set.
 */
struct ZonePairHash {
    size_t operator()(const std::pair<const ZoneState*, const ZoneState*>& p) const noexcept {
        return reinterpret_cast<uintptr_t>(p.first) ^ (reinterpret_cast<uintptr_t>(p.second) << 1);
    }
};

/**
 * @brief Check if a transition is considered internal (tau) under weak semantics.
 * @param t Transition pointer (may be nullptr – not checked here).
 * @return true if action is tau or empty unsynchronized; false otherwise.
 */
inline bool is_tau(const Transition* t){
    return t->action == TA_CONFIG.tau_action_name || (!t->has_synchronization() && t->action.empty());
}

/**
 * @brief Compute the τ-closure (reachable zones using only weak/internal moves).
 *
 * Process:
 *  1. BFS over internal transitions.
 *  2. For every internal edge: apply invariants, elapse time, check guard, apply reset, invariants.
 *  3. Reuse existing canonical zone states via find_zone_state.
 *
 * @param ta Timed automaton.
 * @param start Starting zone state.
 * @return Vector of all reachable zone states (including start).
 */
std::vector<const ZoneState*> tau_closure_raw(const TimedAutomaton& ta, const ZoneState* start){
    std::vector<const ZoneState*> closure; 
    std::queue<const ZoneState*> q; 
    std::unordered_set<const ZoneState*> visited;
    q.push(start); 
    visited.insert(start);
    while(!q.empty()){
        auto z = q.front(); 
        q.pop(); 
        closure.push_back(z);
        auto outs = ta.get_outgoing_transitions(z->location_id);
        for(auto tr: outs){ 
            if(!is_tau(tr)) continue; 
            auto zInv = ta.apply_invariants(z->zone, z->location_id); 
            if(zInv.empty()) continue; 
            auto zUp = ta.time_elapse(zInv); 
            if(zUp.empty()) continue; 
            if(!ta.is_transition_enabled(zUp, *tr)) continue; 
            auto post = ta.apply_transition(zUp, *tr); 
            if(post.empty()) continue; 
            post = ta.apply_invariants(post, tr->to_location); 
            if(post.empty()) continue; 
            const ZoneState* existing = ta.find_zone_state(tr->to_location, post); 
            if(existing && !visited.count(existing)){ 
                visited.insert(existing); 
                q.push(existing);
            }
        }
    }
    return closure;
}

/**
 * @brief Compute weak successors for an observable action (τ* a τ* pattern).
 *
 * Steps:
 *  1. Expand τ-closure pre-set from the start state.
 *  2. For each zone in closure, attempt the observable action.
 *  3. After the action, expand τ-closure again and collect endpoints.
 *  4. Deduplicate resulting zone states.
 *
 * @param ta Timed automaton.
 * @param start Start zone state.
 * @param action Observable action label to match.
 * @return Vector of unique weak reachable zone states.
 */
std::vector<const ZoneState*> weak_observable_successors_raw(const TimedAutomaton& ta,
                                                             const ZoneState* start,
                                                             const std::string& action){
    std::vector<const ZoneState*> result; 
    auto pre = tau_closure_raw(ta, start);
    for(auto z: pre){ 
        auto outs = ta.get_outgoing_transitions(z->location_id); 
        for(auto tr: outs){ 
            if(tr->action != action) continue; 
            auto zInv = ta.apply_invariants(z->zone, z->location_id); 
            if(zInv.empty()) continue; 
            auto zUp = ta.time_elapse(zInv); 
            if(zUp.empty()) continue; 
            if(!ta.is_transition_enabled(zUp, *tr)) continue; 
            auto post = ta.apply_transition(zUp, *tr); 
            if(post.empty()) continue; 
            post = ta.apply_invariants(post, tr->to_location); 
            if(post.empty()) continue; 
            const ZoneState* mid = ta.find_zone_state(tr->to_location, post); 
            if(!mid) continue; 
            auto postTau = tau_closure_raw(ta, mid); 
            result.insert(result.end(), postTau.begin(), postTau.end()); 
        }
    }
    std::unordered_set<const ZoneState*> uniq(result.begin(), result.end()); 
    result.assign(uniq.begin(), uniq.end()); 
    return result;
}

/**
 * @brief Check asymmetric timing compatibility between enabling zones.
 *
 * Constructs Up((Z ∩ Inv) ∩ Guard) for both refined and abstract sides.
 * Uses DBM relation to decide subset/superset.
 * Rules applied:
 *  - Internal (no sync)     : refined ⊆ abstract
 *  - Synchronous send  (!)  : refined ⊆ abstract (must not widen send window)
 *  - Synchronous receive (?) : abstract ⊆ refined (refined may wait longer / allow earlier)
 *
 * @return true if timing relation satisfies RTWBS rule for this action.
 */
bool timing_ok(const TimedAutomaton& refined, const ZoneState* rz, const Transition* rt,
               const TimedAutomaton& abs, const ZoneState* az, const Transition* at){
    auto rInv = refined.apply_invariants(rz->zone, rz->location_id); 
    if(rInv.empty()) return false; 
    auto rUp = refined.time_elapse(rInv); 
    if(rUp.empty()) return false; 
    for(auto &g: rt->guards) dbm_constrain1(rUp.data(), refined.get_dimension(), g.i, g.j, g.value); 
    if(!dbm_close(rUp.data(), refined.get_dimension()) || dbm_isEmpty(rUp.data(), refined.get_dimension())) return false;
    auto aInv = abs.apply_invariants(az->zone, az->location_id); 
    if(aInv.empty()) return false; 
    auto aUp = abs.time_elapse(aInv); 
    if(aUp.empty()) return false; 
    for(auto &g: at->guards) dbm_constrain1(aUp.data(), abs.get_dimension(), g.i, g.j, g.value); 
    if(!dbm_close(aUp.data(), abs.get_dimension()) || dbm_isEmpty(aUp.data(), abs.get_dimension())) return false;
    relation_t rel = dbm_relation(rUp.data(), aUp.data(), refined.get_dimension()); 
    bool r_subset_a = (rel == base_SUBSET || rel == base_EQUAL); 
    bool a_subset_r = (rel == base_SUPERSET || rel == base_EQUAL);
    if(!rt->has_synchronization() && !at->has_synchronization()) return r_subset_a; // internal
    if(rt->has_synchronization() && at->has_synchronization() && rt->channel == at->channel){ 
        if(rt->is_sender && at->is_sender) return r_subset_a; 
        if(rt->is_receiver && at->is_receiver) return a_subset_r; 
    }
    return false;
}
} // namespace

// ===== RTWBSChecker optimisation member implementations =====
const std::vector<const ZoneState*>& RTWBSChecker::tau_closure_cached(const TimedAutomaton& ta, const ZoneState* start){
    auto it = tau_closure_cache_.find(start);
    if(it!=tau_closure_cache_.end()) return it->second;
    auto vec = tau_closure_raw(ta, start);
    auto ins = tau_closure_cache_.emplace(start, std::move(vec));
    return ins.first->second;
}

const std::vector<const ZoneState*>& RTWBSChecker::weak_observable_successors_cached(const TimedAutomaton& ta, const ZoneState* start, const std::string& action){
    WeakKey k{start, action};
    auto it = weak_succ_cache_.find(k);
    if(it!=weak_succ_cache_.end()) return it->second;
    auto vec = weak_observable_successors_raw(ta, start, action);
    auto ins = weak_succ_cache_.emplace(std::move(k), std::move(vec));
    return ins.first->second;
}

void RTWBSChecker::clear_optimisation_state(){
    tau_closure_cache_.clear();
    weak_succ_cache_.clear();
    reverse_deps_.clear();
    relation_.clear();
    while(!worklist_.empty()) worklist_.pop();
}

/**
 * @brief Core RTWBS equivalence (refinement) check between two automata.
 *
 * Algorithm Outline:
 *  1. Ensure zone graphs are (fully) constructed (current implementation builds all states).
 *  2. Seed candidate relation R with all zone pairs sharing the same location id.
 *  3. Iterate elimination: for each pair (r,a) verify every observable refined move
 *     can be matched by some abstract weak move with acceptable timing and related successors.
 *  4. Remove failing pairs; stop at fixed point. Non-empty relation => refinement holds.
 *
 * Correctness Notes:
 *  - This is a greatest fixed point style refinement (monotone elimination).
 *  - Successor validation is optimistic: requires existence of at least one successor pair
 *    already in relation; convergence ensures global consistency.
 *  - Tau behaviours are abstracted via τ-closure in both pre/post phases.
 *
 * Limitations / TODO:
 *  - No counterexample extraction.
 *  - No on-demand zone exploration (eager build may be large).
 *  - Could refine initial seeding using DBM inclusion to reduce search space.
 *  - Could track predecessor dependencies to avoid full scans per iteration.
 *
 * Complexity (worst-case, without memoisation):
 *  O(|R| * T_match) where T_match involves computing weak successors repeatedly.
 */
bool RTWBSChecker::check_rtwbs_equivalence(const TimedAutomaton& refined, const TimedAutomaton& abstract){
    auto start = std::chrono::high_resolution_clock::now();
    clear_optimisation_state();
    const_cast<TimedAutomaton&>(refined).construct_zone_graph();
    const_cast<TimedAutomaton&>(abstract).construct_zone_graph();
    const auto& RZ = refined.get_all_zone_states();
    const auto& AZ = abstract.get_all_zone_states();
    if(RZ.empty() || AZ.empty()) return false;
    // 1) Early pruning seed: only include pairs with same location AND refined zone subset/eq abstract zone
    for(auto &rzp: RZ){
        for(auto &azp: AZ){
            if(rzp->location_id == azp->location_id){
                relation_t rel = dbm_relation(rzp->zone.data(), azp->zone.data(), rzp->dimension);
                if(rel==base_SUBSET || rel==base_EQUAL){
                    PairKey pk{rzp.get(), azp.get()};
                    relation_.insert(pk);
                    worklist_.push(pk);
                }
            }
        }
    }
    if(relation_.empty()) return false;

    // 2) Localised validation loop using worklist and reverse dependencies
    auto validate_pair = [&](const PairKey& pk)->bool{
        auto rZone = pk.r; auto aZone = pk.a;
        auto rOut = refined.get_outgoing_transitions(rZone->location_id);
        for(auto rt: rOut){
            if(is_tau(rt)) continue;
            // Pre-filter abstract transitions by action and sync metadata
            bool matched=false;
            for(auto at: abstract.get_outgoing_transitions(aZone->location_id)){
                if(is_tau(at)) continue;
                if(rt->action != at->action) continue;
                if(!timing_ok(refined,rZone,rt,abstract,aZone,at)) continue;
                const auto& rSuccs = weak_observable_successors_cached(refined, rZone, rt->action);
                const auto& aSuccs = weak_observable_successors_cached(abstract, aZone, at->action);
                bool found=false; PairKey supporting{};
                for(auto rs: rSuccs){
                    for(auto as: aSuccs){
                        if(rs->location_id==as->location_id){
                            PairKey cand{rs,as};
                            if(relation_.count(cand)) { found=true; supporting=cand; break; }
                        }
                    }
                    if(found) break;
                }
                if(found){
                    // record reverse dependency: pk depends on supporting
                    reverse_deps_[supporting].push_back(pk);
                    matched=true; break;
                }
            }
            if(!matched) return false; // attacker wins -> remove
        }
        return true; // all observable transitions matched
    };

    while(!worklist_.empty()){
        PairKey current = worklist_.front(); worklist_.pop();
        // It might have been removed already
        if(!relation_.count(current)) continue;
        if(!validate_pair(current)){
            // remove and enqueue dependents
            relation_.erase(current);
            auto it = reverse_deps_.find(current);
            if(it!=reverse_deps_.end()){
                for(auto &parent: it->second){
                    if(relation_.count(parent)) worklist_.push(parent);
                }
            }
            reverse_deps_.erase(current);
        }
        if(relation_.empty()) break;
    }
    auto end = std::chrono::high_resolution_clock::now();
    last_stats_.refined_states += RZ.size();
    last_stats_.abstract_states += AZ.size();
    last_stats_.simulation_pairs += relation_.size();
    last_stats_.check_time_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    last_stats_.memory_usage_bytes += relation_.size()*sizeof(PairKey);
    return !relation_.empty();
}

/**
 * @brief Pairwise system-level refinement: all corresponding automata must refine.
 * @return true iff every automaton pair satisfies RTWBS refinement.
 */
bool RTWBSChecker::check_rtwbs_equivalence(const System& system_refined, const System& system_abstract){
    if(system_refined.size()!=system_abstract.size()) return false; 
    bool all=true; 
    for(size_t i=0;i<system_refined.size();++i) {
        if(!check_rtwbs_equivalence(system_refined.get_automaton(i), system_abstract.get_automaton(i))) all=false; 
    }
    return all; 
}

/**
 * @brief Detailed system refinement returning per-automaton statistics.
 * @param results Output vector populated with one entry per automaton pair.
 * @return true if all pairs refine.
 */
bool RTWBSChecker::check_rtwbs_equivalence_detailed(const System& system_refined,
                                                    const System& system_abstract,
                                                    std::vector<SystemCheckResult>& results){
    results.clear(); 
    if(system_refined.size()!=system_abstract.size()) return false; 
    bool all=true; 
    const auto& rn=system_refined.get_template_names(); 
    const auto& an=system_abstract.get_template_names(); 
    for(size_t i=0;i<system_refined.size();++i){ 
        bool eq = check_rtwbs_equivalence(system_refined.get_automaton(i), system_abstract.get_automaton(i)); 
        results.push_back(SystemCheckResult{i,rn[i],an[i],eq,get_last_check_statistics()}); 
        if(!eq) all=false; 
    } 
    return all; 
}

} // namespace rtwbs
