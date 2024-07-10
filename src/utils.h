/*
 * -----------------------------------------------------------------------------
 * AUTHORS : Vedad Hadžić, Graz University of Technology, Austria
 *           Simon Tollec, Univ. Paris-Saclay, CEA-List, France
 * DOCUMENT: https://eprint.iacr.org/2024/247
 * -----------------------------------------------------------------------------
 *
 * Copyright 2024, Vedad Hadžić and Simon Tollec
 * Licensed under the Apache License, Version 2.0, see LICENSE for details.
 *
 */

#ifndef VERIFIER_UTILS_H
#define VERIFIER_UTILS_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <set>

#include "json.hpp"
#include "Circuit.h"
#include "vars.h"
#include "Solver.h"

using var_t = cxxsat::var_t;

///////////   Fault_spec_t   ///////////////////////////////////////////////////
// How fault effects are defined:
//  - since transient bit-flip encompasses transient bit-set/reset,
//    we only consider bit-flip effects in the following

#if false
struct fault_spec_t
{
    // 00 = no induce_fault, 01 = bit-flip, 10 = reset, 11 = set
    var_t f0;
    var_t f1;
    fault_spec_t() : f0(cxxsat::solver->new_var()), f1(cxxsat::solver->new_var()) {}
    const var_t is_faulted() const { return f1 | f0; }
    const var_t only_flip() const { return !f1; }
    const var_t only_reset() const { return !f0; }
    const var_t only_set() const { return !(f1 ^ f0); }
    const var_t induce_fault(var_t normal);
};

const var_t fault_spec_t::induce_fault(const var_t normal)
{
    var_t new_value = cxxsat::solver->new_var();
    // 00: equal
    cxxsat::solver->add_clause(f1, f0,  normal, -new_value);
    cxxsat::solver->add_clause(f1, f0, -normal,  new_value);
    // 01: bit-flip
    cxxsat::solver->add_clause(f1, -f0,  normal,  new_value);
    cxxsat::solver->add_clause(f1, -f0, -normal, -new_value);
    // 10: reset
    cxxsat::solver->add_clause(-f1, f0, -new_value);
    // 11: set
    cxxsat::solver->add_clause(-f1, -f0, new_value);
    return new_value;
}
#else
struct fault_spec_t {
    // 0 = no fault, 1 = bit-flip
    var_t f0;
    fault_spec_t() : f0(cxxsat::solver->new_var()) {}
    const var_t is_faulted() const { return f0; }
    const var_t induce_fault(var_t normal);
};
#endif

inline std::ostream& show_diff(std::ostream& out, const std::string& vcd_id, bool val_g, bool val_f)
{
    if (val_f != val_g)
    { out << "bx" << " d" << vcd_id << std::endl; }
    else
    { out << "b" << val_g << " d" << vcd_id << std::endl; }
    return out;
}

std::string replace_all(const std::string& s, const std::string& x, const std::string& y);

void dump_vcd(const std::string& file_name, const Circuit& circ,
              const std::vector<std::unordered_map<signal_id_t, var_t>>& trace_g,
              const std::vector<std::unordered_map<signal_id_t, var_t>>& trace_f,
              const std::string& option = "");

void write_gtkw_savefile(const std::vector<uint32_t>& faulty_initial,
                         const std::vector<uint32_t>& faulty_next,
                         const std::vector<std::unordered_set<signal_id_t>>& partitions,
                         const Circuit& circuit, const std::string& dumpfile);

std::stringstream partition_info(const Circuit& circuit,
                          const std::vector<std::unordered_set<signal_id_t>>& partitions,
                          const std::vector<std::string>& interesting_names);

/*  golden_trace and faulty_trace are initialized with different initial states ;
 *  inputs are the same but internal value of registers are different
 */
void unroll_init_with_faults(const Circuit& circuit,
                             std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                             std::vector<std::unordered_map<signal_id_t, var_t>>& faulty_trace,
                             const std::unordered_set<signal_id_t>& faultable_sigs,
                             std::vector<std::unordered_map<signal_id_t, fault_spec_t>>& faults);

void unroll_with_faults(const Circuit& circuit,
                        std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                        std::vector<std::unordered_map<signal_id_t, var_t>>& faulty_trace,
                        const std::unordered_set<signal_id_t>& faultable_sigs,
                        std::vector<std::unordered_map<signal_id_t, fault_spec_t>>& faults,
                        const std::unordered_set<signal_id_t>& alert_signals);

void init_constants(std::unordered_map<signal_id_t, var_t>& state);

/*  Assert invariants on signals defined in the body of the function.
 *  This applies to the golden trace only
 */
void assert_invariants_at_step(const Circuit& circuit,
                               const std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                               const std::unordered_map<std::string, std::vector<bool>> invariant_list,
                               uint32_t step);

void assume_no_comb_fault_if_not_connected_to_outputs(const Circuit& circuit,
                const std::unordered_map<signal_id_t, fault_spec_t>& comb_faults);

/*  Assert invariants on signals defined in the body of the function.
 *  This applies to both golden_trace and faulty_trace
 */
void assert_no_alert_at_step(const Circuit& circuit,
                             const std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                             const std::vector<std::unordered_map<signal_id_t, var_t>>& faulty_trace,
                             const std::unordered_map<std::string, std::vector<bool>>& alert_list,
                             uint32_t step);


std::vector<std::unordered_set<signal_id_t>> init_partitions_from_file(const Circuit& circuit,
                                                                       const std::string file_name);

std::vector<std::unordered_set<signal_id_t>> init_partitions_from_scratch(const Circuit& circuit);

std::unordered_set<signal_id_t> compute_faultable_signals(
    const Circuit& circuit,
    const std::vector<std::string>& f_included_prefix,
    const std::vector<std::string>& f_excluded_prefix,
    const std::vector<signal_id_t>& f_excluded_signals,
    const bool exclude_inputs);

std::unordered_set<uint32_t> get_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const uint32_t part_idx);

std::unordered_set<uint32_t> get_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const signal_id_t& sig);

bool at_least_2_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const uint32_t part_idx);

bool at_least_2_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const signal_id_t& sig);

std::stringstream optim_at_least_2_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const std::unordered_map<signal_id_t, fault_spec_t>& initial_comb_faults,
    const std::vector<var_t>& initial_partitions_diff);

#endif // VERIFIER_UTILS_H
