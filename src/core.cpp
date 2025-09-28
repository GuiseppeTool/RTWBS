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
#include <unordered_map>
#include <iostream>
#include <future>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
namespace rtwbs {

namespace { // anonymous helpers (not part of public API)


// Global cooperative-cancel flag for long-running computations in this TU.
// All checker instances in this process will honor cancellation when set.
static std::atomic<bool>* RTWBS_CANCEL_FLAG = nullptr;
inline bool is_cancelled() {
    return RTWBS_CANCEL_FLAG && RTWBS_CANCEL_FLAG->load(std::memory_order_relaxed);
}

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
    // Internal moves are ONLY unsynchronized and labeled as tau (or empty)
    return (!t->has_synchronization()) && (t->action == TA_CONFIG.tau_action_name || t->action.empty());
}

/**
 * @brief Compute the τ-closure (reachable zones using only weak/internal moves).
 *
 * Process:
 *  1. BFS over internal transitions.
 *  2. For every internal edge: apply invariants, elapse time, re-apply invariants, apply reset+guards, invariants.
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
        if (is_cancelled()) return closure;
        auto z = q.front(); 
        q.pop(); 
        closure.push_back(z);
        auto outs = ta.get_outgoing_transitions(z->location_id);
        for(auto tr: outs){ 
            if (is_cancelled()) return closure;
            if(!is_tau(tr)) continue; 
            auto zInv = ta.apply_invariants(z->zone, z->location_id);
            if(zInv.empty()) continue;
            auto zUp = ta.time_elapse(zInv);
            if(zUp.empty()) continue;
            // Re-apply invariants after Up to avoid enabling via invariant violations
            auto ready = ta.apply_invariants(zUp, z->location_id);
            if(ready.empty()) continue;
            // Direct attempt: apply transition; guards+resets handled inside
            auto post = ta.apply_transition(ready, *tr);
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
        if (is_cancelled()) break;
        auto outs = ta.get_outgoing_transitions(z->location_id); 
        for(auto tr: outs){ 
            if (is_cancelled()) break;
            if(tr->action != action) continue; 
            auto zInv = ta.apply_invariants(z->zone, z->location_id);
            if(zInv.empty()) continue;
            auto zUp = ta.time_elapse(zInv);
            if(zUp.empty()) continue;
            auto ready = ta.apply_invariants(zUp, z->location_id);
            if(ready.empty()) continue;
            auto post = ta.apply_transition(ready, *tr);
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
    // Keep invariants enforced during delay
    auto rReady = refined.apply_invariants(rUp, rz->location_id);
    if(rReady.empty()) return false;

    for(auto &g: rt->guards) dbm_constrain1(rReady.data(), refined.get_dimension(), g.i, g.j, g.value);
    bool guards_ok_in_r=true;
    if(!dbm_close(rReady.data(), refined.get_dimension()) || dbm_isEmpty(rReady.data(), refined.get_dimension()))
    {
        guards_ok_in_r= false;
    }


    auto aInv = abs.apply_invariants(az->zone, az->location_id);
    if(aInv.empty()) return false;
    auto aUp = abs.time_elapse(aInv);
                
    if(aUp.empty()) return false;
    // Keep invariants enforced during delay
    
    auto aReady = abs.apply_invariants(aUp, az->location_id);
    if(aReady.empty()) return false;


    for(auto &g: at->guards) dbm_constrain1(aReady.data(), abs.get_dimension(), g.i, g.j, g.value);
    bool guards_ok_in_a=true;
    if(!dbm_close(aReady.data(), abs.get_dimension()) || dbm_isEmpty(aReady.data(), abs.get_dimension())){
        guards_ok_in_a= false;
    }
    if(guards_ok_in_r and guards_ok_in_a){

    }
    if(guards_ok_in_r && guards_ok_in_a){
        // Both can move → check zone relation
        relation_t rel = dbm_relation(rReady.data(), aReady.data(), refined.get_dimension()); 
        bool r_subset_a = (rel == base_SUBSET || rel == base_EQUAL); 
        bool a_subset_r = (rel == base_SUPERSET || rel == base_EQUAL);

        if(!rt->has_synchronization() && !at->has_synchronization())
            return r_subset_a; // internal
        if(rt->has_synchronization() && at->has_synchronization() && rt->channel == at->channel){ 
            if(rt->is_sender && at->is_sender) return r_subset_a; 
            if(rt->is_receiver && at->is_receiver) return a_subset_r; 
        }
        return false; // guards ok but cannot match
    }
    else if(!guards_ok_in_r && !guards_ok_in_a){
        // Both cannot move → bisimulation trivially holds
        return true;
    }
    else {
        // Only one can move → bisimulation fails
        return false;
    }
    //
    //relation_t rel = dbm_relation(rReady.data(), aReady.data(), refined.get_dimension()); 
    //bool r_subset_a = (rel == base_SUBSET || rel == base_EQUAL); 
    //bool a_subset_r = (rel == base_SUPERSET || rel == base_EQUAL);
    //if(!rt->has_synchronization() && !at->has_synchronization()) return r_subset_a; // internal
    //if(rt->has_synchronization() && at->has_synchronization() && rt->channel == at->channel){ 
    //    if(rt->is_sender && at->is_sender) return r_subset_a; 
    //    if(rt->is_receiver && at->is_receiver) return a_subset_r; 
    //}
    //return false;
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
    WeakKey k{start->location_id, action};
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
 * @brief Core RTWBS simulation (refinement) check between two automata.
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



bool RTWBSChecker::check_rtwbs_simulation(const TimedAutomaton& refined, const TimedAutomaton& abstract){
    auto start = std::chrono::high_resolution_clock::now();
    clear_optimisation_state();
    if (is_cancelled()) return false;
    const_cast<TimedAutomaton&>(refined).construct_zone_graph();
    const_cast<TimedAutomaton&>(abstract).construct_zone_graph();
    const auto& RZ = refined.get_all_zone_states();
    const auto& AZ = abstract.get_all_zone_states();
    if(RZ.empty() || AZ.empty()) return false;
    // 1) Early pruning seed: only include pairs with same location AND refined zone subset/eq abstract zone
    
    if(false){
        for(auto &rzp: RZ){
            for(auto &azp: AZ){
                if(rzp->location_id == azp->location_id){
                    relation_t rel = dbm_relation(rzp->zone.data(), azp->zone.data(), rzp->dimension);
                    if(rel==base_SUBSET || rel==base_EQUAL){
                        PairKey pk{rzp->location_id, azp->location_id};
                        relation_.insert(pk);
                        worklist_.push(pk);
                    }
                }
            }
        }
    }
    if(relation_.empty()) return false;

    // 2) Localised validation loop using worklist and reverse dependencies
    auto validate_pair = [&](const PairKey& pk)->bool{
        auto rZone = pk.r; auto aZone = pk.a;
        auto rOut = refined.get_outgoing_transitions(refined.get_zone_state(rZone)->location_id);
        for(auto rt: rOut){
            if(is_tau(rt)) continue;
            // Compute enabled refined weak successors for this action; if none, skip this transition
            const auto& rSuccs = weak_observable_successors_cached(refined, refined.get_zone_state(rZone), rt->action);
            if(rSuccs.empty()) continue; // not enabled at this zone
            // Pre-filter abstract transitions by action
            bool matched=false;
            for(auto at: abstract.get_outgoing_transitions(abstract.get_zone_state(aZone)->location_id)){
                if(is_tau(at)) continue;
                if(rt->action != at->action) continue;
                // Abstract side must also be enabled for this action
                const auto& aSuccs = weak_observable_successors_cached(abstract, abstract.get_zone_state(aZone), at->action);
                if(aSuccs.empty()) continue;
                if(!timing_ok(refined,refined.get_zone_state(rZone),rt,abstract,abstract.get_zone_state(aZone),at)) continue;
                bool found=false; PairKey supporting{};
                for(auto rs: rSuccs){
                    for(auto as: aSuccs){
                        if(rs->location_id==as->location_id){
                            PairKey cand{refined.get_state_id(rs),abstract.get_state_id(as)};
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
            if(!matched) return false; // some enabled refined move has no match
        }
        return true; // all observable transitions matched
    };
    
    while(!worklist_.empty()){
        if (is_cancelled()) { relation_.clear(); break; }
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
bool RTWBSChecker::check_rtwbs_simulation(const System& system_refined, const System& system_abstract){
    if(system_refined.size()!=system_abstract.size()) return false; 
    bool all=true; 
    for(size_t i=0;i<system_refined.size();++i) {
        if(!check_rtwbs_simulation(system_refined.get_automaton(i), system_abstract.get_automaton(i))) all=false; 
    }
    return all; 
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





bool RTWBSChecker::check_rtwbs_equivalence(const TimedAutomaton& refined, const TimedAutomaton& abstract, bool use_omp){
    auto start = std::chrono::high_resolution_clock::now();
    clear_optimisation_state();
    if (is_cancelled()) return false;
    const_cast<TimedAutomaton&>(refined).construct_zone_graph();
    const_cast<TimedAutomaton&>(abstract).construct_zone_graph();
    const auto& RZ = refined.get_all_zone_states();
    const auto& AZ = abstract.get_all_zone_states();
    if(RZ.empty() || AZ.empty()) return false;
    DEV_PRINT( "Thread " << std::this_thread::get_id() << " is doing equivalence check\n for "<< refined.get_name() << " and " << abstract.get_name() << std::endl);

    // 1) Early pruning seed: only include pairs with same location AND refined zone subset/eq abstract zone
    for(auto &rzp: RZ){
        if (is_cancelled()) return false;
        for(auto &azp: AZ){
            if (is_cancelled()) return false;
            if(rzp->location_id == azp->location_id){
                relation_t rel = dbm_relation(rzp->zone.data(), azp->zone.data(), rzp->dimension);
                if(rel==base_SUBSET || rel==base_EQUAL){
                    PairKey pk{refined.get_state_id(rzp.get()),
                                abstract.get_state_id(azp.get())};
                    relation_.insert(pk);
                    worklist_.push(pk);
                }
            }
        }
    }
    if(relation_.empty()) return false;

    // 2) Localised validation loop using worklist and reverse dependencies
    //    Now symmetric: both refined→abstract and abstract→refined must match (bisimulation)
    // Thread-safe validate_pair for OpenMP: collects reverse_deps_ updates in a local vector
    auto validate_pair = [&](const PairKey& pk, const std::unordered_set<rtwbs::RTWBSChecker::PairKey, rtwbs::RTWBSChecker::PairKeyHash>& relation_, std::vector<std::pair<PairKey, PairKey>>* local_reverse_deps_updates = nullptr)->bool{
        auto rZone = pk.r; auto aZone = pk.a;
        // Forward direction: refined -> abstract
        {
            auto rOut = refined.get_outgoing_transitions(refined.get_zone_state(rZone)->location_id);
            for(auto rt: rOut){
                if(is_tau(rt)) continue;
                const auto& rSuccs = weak_observable_successors_cached(refined, refined.get_zone_state(rZone), rt->action);
                if(rSuccs.empty()) continue; // transition not enabled from rZone
                
                bool matched=false;
                for(auto at: abstract.get_outgoing_transitions(abstract.get_zone_state(aZone)->location_id)){
                    if(is_tau(at)) continue;
                    if(rt->action != at->action) continue;
                    // Cheap sync precheck to avoid expensive DBM timing_ok when impossible
                    bool rs = rt->has_synchronization();
                    bool asy = at->has_synchronization();
                    if(rs != asy) continue;
                    if(rs){
                        if(rt->channel != at->channel) continue;
                        if(rt->is_sender != at->is_sender) continue;
                        if(rt->is_receiver != at->is_receiver) continue;
                    }
                    const auto& aSuccs = weak_observable_successors_cached(abstract, abstract.get_zone_state(aZone), at->action);
                    if(aSuccs.empty()){                         
                        continue;} // not enabled on abstract side
                    if(!timing_ok(refined, refined.get_zone_state(rZone), rt, abstract, abstract.get_zone_state(aZone), at))
                    {
                        continue; // timing incompatible
                    }
                    // Index abstract successors by location to reduce O(m*n)
                    std::unordered_map<int, std::vector<const ZoneState*>> aByLoc;
                    aByLoc.reserve(aSuccs.size());
                    for(auto as: aSuccs) aByLoc[as->location_id].push_back(as);
                    bool found=false; PairKey supporting{};
                    for(auto rsucc: rSuccs){
                        auto it = aByLoc.find(rsucc->location_id);
                        if(it==aByLoc.end()) continue;
                        for(auto as: it->second){
                            PairKey cand{refined.get_state_id(rsucc),abstract.get_state_id(as)};
                            if(relation_.count(cand)) { found=true; supporting=cand; break; }
                        }
                        if(found) break;
                    }
                    if(found){
                        // record reverse dependency: pk depends on supporting
                        if (local_reverse_deps_updates) {
                            local_reverse_deps_updates->emplace_back(supporting, pk);
                        } else {
                            reverse_deps_[supporting].push_back(pk);
                        }
                        matched=true; break;
                    }
                }
                if(!matched) return false; // enabled refined move has no match
            }
        }
        
        // Backward direction: abstract -> refined
        {
            auto aOut = abstract.get_outgoing_transitions(abstract.get_zone_state(aZone)->location_id);
            for(auto at: aOut){
                if(is_tau(at)) continue;
                const auto& aSuccs = weak_observable_successors_cached(abstract, abstract.get_zone_state(aZone), at->action);
                if(aSuccs.empty()) continue; // not enabled from aZone
                bool matched=false;
                for(auto rt: refined.get_outgoing_transitions(refined.get_zone_state(rZone)->location_id)){
                    if(is_tau(rt)) continue;
                    if(at->action != rt->action) continue;
                    // Cheap sync precheck (mirror)
                    bool asy = at->has_synchronization();
                    bool rs = rt->has_synchronization();
                    if(asy != rs) continue;
                    if(asy){
                        if(at->channel != rt->channel) continue;
                        if(at->is_sender != rt->is_sender) continue;
                        if(at->is_receiver != rt->is_receiver) continue;
                    }
                    const auto& rSuccs = weak_observable_successors_cached(refined, refined.get_zone_state(rZone), rt->action);
                    if(rSuccs.empty()){
                        continue;
                    }  // not enabled on refined side
                    if(!timing_ok(abstract, abstract.get_zone_state(aZone), at, refined, refined.get_zone_state(rZone), rt)) continue; // mirror timing
                    // Index refined successors by location
                    std::unordered_map<int, std::vector<const ZoneState*>> rByLoc;
                    rByLoc.reserve(rSuccs.size());
                    for(auto rsucc: rSuccs) rByLoc[rsucc->location_id].push_back(rsucc);
                    bool found=false; PairKey supporting{};
                    for(auto as: aSuccs){
                        auto it = rByLoc.find(as->location_id);
                        if(it==rByLoc.end()) continue;
                        for(auto rsucc: it->second){
                            PairKey cand{refined.get_state_id(rsucc),abstract.get_state_id(as)};
                            if(relation_.count(cand)) { found=true; supporting=cand; break; }
                        }
                        if(found) break;
                    }
                    if(found){
                        if (local_reverse_deps_updates) {
                            local_reverse_deps_updates->emplace_back(supporting, pk);
                        } else {
                            reverse_deps_[supporting].push_back(pk);
                        }
                        matched=true; break;
                    }
                }
                

                if(!matched) return false; // enabled abstract move has no match
            }
        }
        return true; // all observable transitions matched in both directions
    };

    if(!use_omp){
        while(!worklist_.empty()){
            if (is_cancelled()) { relation_.clear(); break; }
            PairKey current = worklist_.front(); worklist_.pop();
            // It might have been removed already
            if(!relation_.count(current)) continue;
            bool valid = validate_pair(current, relation_);
            
            
            if(!valid){
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
        
    }else{
        // transform it in a lambda
        auto get_batch = [](std::queue<PairKey>& worklist, size_t batch_size) {
            std::vector<PairKey> batch;
            batch.reserve(batch_size);
            for (size_t i = 0; i < batch_size && !worklist.empty(); ++i) {
                batch.push_back(worklist.front());
                worklist.pop();
            }
            return batch;
        };
        
        while (!worklist_.empty()) {
            // 1. Extract a batch of pairs from worklist_
            size_t batch_size = std::max(static_cast<size_t>(1), worklist_.size() / (2 * std::thread::hardware_concurrency()));
            std::vector<PairKey> batch = get_batch(worklist_, batch_size);


            // 2. Parallel for each pair in batch (only read shared containers)
            std::vector<PairKey> to_remove;
            std::vector<std::pair<PairKey, PairKey>> all_reverse_deps_updates;
            const auto& relation_snapshot = relation_; // snapshot for thread safety
            #pragma omp parallel
            {
                std::vector<PairKey> local_remove;
                std::vector<std::pair<PairKey, PairKey>> local_reverse_deps_updates;

                #pragma omp for nowait
                for (size_t i = 0; i < batch.size(); ++i) {
                    PairKey current = batch[i];
                    if (!relation_snapshot.count(current)) continue;
                    if (!validate_pair(current, relation_, &local_reverse_deps_updates)) {
                        local_remove.push_back(current);
                    }
                }

                // Merge thread-local results at the end of parallel region
                #pragma omp critical
                {
                    to_remove.insert(to_remove.end(),
                                    std::make_move_iterator(local_remove.begin()),
                                    std::make_move_iterator(local_remove.end()));

                    all_reverse_deps_updates.insert(all_reverse_deps_updates.end(),
                                    std::make_move_iterator(local_reverse_deps_updates.begin()),
                                    std::make_move_iterator(local_reverse_deps_updates.end()));
                }
            }
            
            // 3. Synchronize: merge reverse_deps_ updates (main thread only)
            for (const auto& update : all_reverse_deps_updates) {
                reverse_deps_[update.first].push_back(update.second);
            }
            // For each pair to remove, enqueue its dependents and erase from reverse_deps_
            for (auto& pk : to_remove) {
                relation_.erase(pk);
                auto it = reverse_deps_.find(pk);
                if (it != reverse_deps_.end()) {
                    for (auto& parent : it->second) {
                        if (relation_.count(parent)) worklist_.push(parent);
                    }
                }
                reverse_deps_.erase(pk);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
        last_stats_.refined_states += RZ.size();
        last_stats_.abstract_states += AZ.size();
        last_stats_.simulation_pairs += relation_.size();
        last_stats_.check_time_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        last_stats_.memory_usage_bytes += relation_.size()*sizeof(PairKey);
        DEV_PRINT( "Thread " << std::this_thread::get_id() << " finished equivalence check\n for "<< refined.get_name() << " and " << abstract.get_name() << std::endl);        
        for (const auto& p : relation_)
        {
            if (p.r == 0 && p.a == 0)

                return true;
        }
        return false;
        
}


bool RTWBSChecker::check_rtwbs_equivalence__(const System& system_refined, const System& system_abstract, bool use_openmp)
{
       bool all = true; 
    size_t total = system_refined.size();
    int barWidth = 70;
    std::vector<bool> results(total, false);
    for(size_t i = 0; i < total; ++i) {
        if (is_cancelled()) { all = false; break; }
                // Run equivalence check
                if(!check_rtwbs_equivalence(system_refined.get_automaton(i), system_abstract.get_automaton(i), use_openmp))
                    all = false; 

                // Report result of current automaton
                

                // Update progress bar
                float progress = float(i + 1) / total; // completed fraction

                std::cout << "[";
                int pos = barWidth * progress;
                for (int j = 0; j < barWidth; ++j) {
                    if (j < pos) std::cout << "=";
                    else if (j == pos) std::cout << ">";
                    else std::cout << " ";
                }
                std::cout << "] " << int(progress * 100.0) <<"% ";
                std::cout <<"Automaton pair " << i << ": " 
                        << (all ? "EQUIVALENT" : "DIFFERENT") << " \r";
                std::cout.flush();
                results[i] = all;
            }

            std::cout << std::endl; // move to next line after progress bar finishes
            for (size_t i = 0; i < total; ++i) {
                std::cout << "Automaton pair " << i << ": " 
                        << (results[i] ? "EQUIVALENT" : "DIFFERENT") << std::endl;
            }

            return all; 
}


/**
 * @brief Pairwise system-level refinement: all corresponding automata must refine.
 * @return true iff every automaton pair satisfies RTWBS refinement.
 */
bool RTWBSChecker::check_rtwbs_equivalence(const System& system_refined, const System& system_abstract, RunningMode parallel_mode, size_t num_workers, long timeout_ms) {
    if(system_refined.size() != system_abstract.size()) 
        return false; 

    // Cooperative-cancel with watchdog if timeout is set
    std::atomic<bool> cancel_flag{false};
    std::atomic<bool> done_flag{false};
    std::thread watchdog;
    std::mutex watchdog_mtx;
    std::condition_variable watchdog_cv;
    bool installed_cancel_flag = false;
    if (timeout_ms >= 0) {
        // Install global cancel flag pointer before any work starts
        RTWBS_CANCEL_FLAG = &cancel_flag;
        installed_cancel_flag = true;
        watchdog = std::thread([timeout_ms, &cancel_flag, &done_flag, &watchdog_mtx, &watchdog_cv]() {
            if (timeout_ms == 0) {
                // immediate cancel
                cancel_flag.store(true, std::memory_order_relaxed);
                std::cout << "TIME OUT HAPPENED, I WAS CHECKING IF AUZTOMATA" << std::endl;
                return;
            }
            std::unique_lock<std::mutex> lk(watchdog_mtx);
            bool finished = watchdog_cv.wait_for(
                lk,
                std::chrono::milliseconds(timeout_ms),
                [&]{ return done_flag.load(std::memory_order_relaxed); }
            );
            if (!finished) {
                cancel_flag.store(true, std::memory_order_relaxed);
                std::cout << "TIME OUT HAPPENED, I WAS CHECKING IF AUZTOMATA" << std::endl;
            }
        });
    }

    auto join_watchdog = [&]() {
        done_flag.store(true, std::memory_order_relaxed);
        watchdog_cv.notify_all();
        if (watchdog.joinable()) watchdog.join();
        if (installed_cancel_flag && RTWBS_CANCEL_FLAG == &cancel_flag) {
            RTWBS_CANCEL_FLAG = nullptr;
        }
    };

    auto run_work = [&]() -> bool {
        if (parallel_mode == RunningMode::SERIAL) {
            return check_rtwbs_equivalence__(system_refined, system_abstract, false);
        }

        if (parallel_mode == RunningMode::OPENMP) {
            std::cout << "Running it in openmp mode" << std::endl;
            return check_rtwbs_equivalence__(system_refined, system_abstract, true);
        }

        // Pre-construct all zone graphs sequentially (avoid concurrent mutations)
        for (size_t i = 0; i < system_refined.size(); ++i) {
            if (is_cancelled()) return false;
            auto& r = const_cast<TimedAutomaton&>(system_refined.get_automaton(i));
            auto& a = const_cast<TimedAutomaton&>(system_abstract.get_automaton(i));
            r.construct_zone_graph();
            a.construct_zone_graph();
        }
        bool all_local = true;
        std::vector<std::future<std::pair<bool, rtwbs::CheckStatistics>>> futures;
        ThreadPool pool(num_workers);
        futures.reserve(system_refined.size());
        for (size_t i = 0; i < system_refined.size(); ++i) {
            futures.push_back(pool.enqueue([&system_refined, &system_abstract, i]() {
                RTWBSChecker local;
                bool correct = local.check_rtwbs_equivalence(
                    system_refined.get_automaton(i),
                    system_abstract.get_automaton(i)
                );
                return std::make_pair(correct, local.get_last_check_statistics());
            }));
        }
        for (auto& fut : futures) {
            if (is_cancelled()) { all_local = false; break; }
            auto [correct, stats] = fut.get();
            if (!correct) all_local = false;
            auto old_time = this->last_stats_.check_time_ms;
            this->last_stats_ += stats;
            this->last_stats_.check_time_ms = std::max(old_time, stats.check_time_ms);
        }
        return all_local;
    };

    bool res = run_work();
    join_watchdog();
    // Check if canceled
    if (cancel_flag.load(std::memory_order_relaxed)) {
        this->last_stats_.check_time_ms = timeout_ms;
        throw rtwbs::TimeoutException("Operation timed out!");
    }

    return res;

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
