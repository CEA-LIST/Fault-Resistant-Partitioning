/*
 * -----------------------------------------------------------------------------
 * AUTHORS : Vedad Hadžić, Graz University of Technology, Austria
 *           Simon Tollec, Univ. Paris-Saclay, CEA-List, France
 * DOCUMENT: https://eprint.iacr.org/2024/247
 * -----------------------------------------------------------------------------
 *
 * Copyright 2024, Commissariat à l'énergie atomique et aux énergies
 * alternatives (CEA), France and Graz University of Technology, Austria
 *
 * Licensed under the Apache License, Version 2.0, see LICENSE for details.
 *
 */

#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "Cell.h"
#include "Circuit.h"
#include "Solver.h"
#include "utils.h"
#include "config.h"
#include "vars.h"
#include "json.hpp"

#define MAX_ITER 2000
#define SAT_TIMEOUT 30

using var_t = cxxsat::var_t;


void check_k_fault_resistant_partitioning(std::string config_name)
{
    // Import configuration from file
    config_t CONF("config/config_file.json", config_name);
    std::ofstream out(CONF.dump_path + "/log");

    Circuit* circuit = new Circuit(CONF.design_path, CONF.design_name);

    // Extract subcircuit if needed.
    if (CONF.subcircuit) {
        Circuit* subcircuit = new Circuit(*circuit,
            CONF.subcircuit_interface_path, CONF.subcircuit_interface_name);
        delete circuit;
        circuit = subcircuit;
    }

    circuit->build_adjacent_lists();
    out << circuit->stats().str();

    // Initial circuit's registers partitioning from scratch/file
    std::vector<std::unordered_set<signal_id_t>> partitions;
    if (CONF.initial_partition_path.empty()) {
        partitions = init_partitions_from_scratch(*circuit);
    } else {
        partitions = init_partitions_from_file(*circuit, CONF.initial_partition_path);
    }

    out << partition_info(*circuit, partitions, CONF.interesting_names).str();

    // Collect alert signals in the circuit from the provided `alert_list`
    std::unordered_set<signal_id_t> alert_signals;
    for (const auto& alert : CONF.alert_list)
    {
        const std::vector<signal_id_t>& outs = (*circuit)[alert.first];
        for (const auto& o : outs) alert_signals.emplace(o);
    }

    // Collect faultable signals
    std::unordered_set<signal_id_t> faultable_sigs = compute_faultable_signals(
        *circuit, CONF.f_included_prefix, CONF.f_excluded_prefix,
        CONF.f_excluded_signals, CONF.exclude_inputs);


    // Set time format for dumped files
    srand(42);
    std::chrono::time_point start_time = std::chrono::system_clock::now();
    std::time_t st = std::chrono::system_clock::to_time_t(start_time);
    char time_str[100];
    std::strftime(time_str, 100, "%y.%m.%d@%H:%M:%S", std::localtime(&st));

    ////////////////////////////////////////////////////////////////////////////
    //      Procedure 1 -- Find partitions
    ////////////////////////////////////////////////////////////////////////////
    // CONF.k         :   maximal number of faults (i.e., attack order)
    // k_faults       :   total number of faults in current evaluation
    // k_f_part       :   number of faulty partitions   
    // k_f_comb       :   total number of combinational faults
    // k_f_comb_init  :   number of combinational faults at the first clock cycle
    // k_f_comb_next  :   number of combinational faults at the next clock cycles

    uint32_t solver_iter = 0;
    
    if (CONF.procedure != PROC_2) {

        ////////////////////////////////////////////////////////////////////////////
        //      Unroll the circuit max(1,`DELAY`) times
        ////////////////////////////////////////////////////////////////////////////
        // - Unroll the golden/faulty execution traces
        // - Faults in registers are possible due to their unconstrained initial state
        // - Faults in combinational logic is inserted while unrolling

        // Initialize golden/faulty traces which are a sequence of circuit states.
        std::vector<std::unordered_map<signal_id_t, var_t>> golden_trace;
        std::vector<std::unordered_map<signal_id_t, var_t>> faulty_trace;
        std::vector<std::unordered_map<signal_id_t, fault_spec_t>> comb_faults;

        cxxsat::solver = new cxxsat::Solver();

        for (uint32_t cycle = 0; cycle <= std::max(uint32_t(1), CONF.delay); cycle++)
        {
            if (cycle == 0)
            {
                unroll_init_with_faults(*circuit, golden_trace, faulty_trace,
                                        faultable_sigs, comb_faults);
                // Assume invariant on golden trace
                assert_invariants_at_step(*circuit, golden_trace, CONF.invariant_list, 0);               
            } else {
                unroll_with_faults(*circuit, golden_trace, faulty_trace,
                                faultable_sigs, comb_faults, alert_signals);
            }

            // Assume no alert at each step 
            assert_no_alert_at_step(*circuit, golden_trace, faulty_trace, CONF.alert_list, cycle);
        }

        assert(comb_faults.size() == 1 + std::max(uint32_t(1), CONF.delay));

        ////////////////////////////////////////////////////////////////////////////
        //      Build vectors of partition differences at cycle 0 and 1
        ////////////////////////////////////////////////////////////////////////////
        std::array<std::unordered_map<signal_id_t, var_t>, 2> seq_faults;
        std::array<std::vector<var_t>, 2> partitions_diff;
        for (uint32_t cycle = 0; cycle <= 1; cycle++)
        {
            const auto& golden_state = golden_trace.at(cycle);
            const auto& faulty_state = faulty_trace.at(cycle);
            auto& curr_diff = partitions_diff.at(cycle);
            auto& curr_fault = seq_faults.at(cycle);

            for (const auto& partition : partitions)
            {
                std::vector<var_t> current_partition_diff;
                for (const auto& sig : partition)
                {
                    const auto& it_g = golden_state.find(sig);
                    const auto& it_f = faulty_state.find(sig);
                    assert(it_g != golden_state.end());
                    assert(it_f != faulty_state.end());
                    var_t var = it_g->second ^ it_f->second;
                    current_partition_diff.push_back(var);
                    curr_fault.emplace(sig, var);
                }
                curr_diff.push_back(cxxsat::solver->make_or(current_partition_diff));
            }
        }

        ////////////////////////////////////////////////////////////////////////////
        //      Build vectors of combinational faults at cycle 0 and 1:d
        ////////////////////////////////////////////////////////////////////////////
        std::array<std::vector<var_t>, 2> comb_fault_vars;
        for (uint32_t cycle = 0; cycle < comb_faults.size(); cycle++)
        {
            for (const auto& m_sig_fault : comb_faults.at(cycle))
                comb_fault_vars.at(cycle ? 1 : 0).push_back(m_sig_fault.second.is_faulted());
        }

        const auto start_proc1{std::chrono::steady_clock::now()};

        std::unordered_set<signal_id_t> enumerate_comb_faults;

        // Print banner
        out << std::endl << std::string(80, '*') << std::endl;
        out << std::string(20, ' ') << "Procedure 1 -- Build partitions";
        out << std::endl << std::string(80, '*') << std::endl;

        for (int k_faults = (CONF.increasing_k) ? 1 : CONF.k; k_faults <= CONF.k; k_faults++)
        {
            // Set the iteration loop of comb faults to 0 if needed
            int max_k_f_comb = (CONF.f_gates == SEQ) ? 0 : k_faults;
            for (int k_f_comb = max_k_f_comb; k_f_comb >= 0; k_f_comb--)
            {
                for (int k_f_comb_next = 0; k_f_comb_next <= std::min(k_faults - 1, k_f_comb); k_f_comb_next++)
                {
                    uint32_t k_f_part = k_faults - k_f_comb;
                    uint32_t k_f_comb_init = k_f_comb - k_f_comb_next;

                    // Print info banner for current analysis
                    out << std::string(80, '-') << std::endl;
                    out << "Partitioning for " << k_f_part << "/" << partitions.size();
                    out << " faulty partitions," << std::endl;
                    out << k_f_comb_init << "/" << comb_fault_vars.at(0).size();
                    out << " combinational faults at initial state," << std::endl;
                    out << "and " << k_f_comb_next << "/" << comb_fault_vars.at(1).size();
                    out << " combinational faults in the following clock cycles." << std::endl;
                    out << std::string(80, '-') << std::endl;

                    // Reset solver state to SAT
                    cxxsat::Solver::state_t res = cxxsat::Solver::state_t::STATE_SAT;


                    // Iterate until a fixed point for the current partitioning analysis
                    for (solver_iter++;solver_iter<MAX_ITER;solver_iter++)
                    {
                        /////////////    OPTIM (at least 2 conn parts)     /////////
                        if (CONF.optim_atleast2) {
                            out << optim_at_least_2_conn_parts(*circuit, partitions,
                                                comb_faults.at(0), partitions_diff.at(0)).str();
                        }

                        ///////////////////     ASSUMPTIONS     ///////////////////////

                        // Initially, at most `k_f_comb_init` comb faults
                        cxxsat::solver->assume(
                            cxxsat::solver->make_at_most(comb_fault_vars.at(0), k_f_comb_init));

                        // Next states, at most `k_f_comb_next` comb faults on alert signals
                        cxxsat::solver->assume(
                            cxxsat::solver->make_at_most(comb_fault_vars.at(1), k_f_comb_next));

                        // Initially, at most `k_f_part` partitions faulted
                        cxxsat::solver->assume(
                            cxxsat::solver->make_at_most(partitions_diff.at(0), k_f_part));

                        // Next state, at least `k_f_part + k_f_comb_init` partitions faulted
                        cxxsat::solver->assume(
                            cxxsat::solver->make_at_least(partitions_diff.at(1), k_faults + 1));

                        // Assume no comb faults that we already enumerated
                        if (CONF.enumerate_exploitable) {
                            out << std::endl << "Enumerate exploitable faults: ";
                            for (const signal_id_t& sig : enumerate_comb_faults)
                            {
                                out << static_cast<uint32_t>(sig) << " ";
                                const auto& f = comb_faults.at(0).find(sig);
                                assert(f != comb_faults.at(0).end());
                                cxxsat::solver->add_clause(!f->second.is_faulted());
                            }
                            out << std::endl;
                        }

                        out << std::endl << "  Running solver " << solver_iter << ": " << std::flush;

                        const auto start_check{std::chrono::steady_clock::now()};
                        res = cxxsat::solver->check();
                        const auto end_check{std::chrono::steady_clock::now()};

                        const auto check_time = end_check - start_check;
                        uint32_t check_time_ms =
                            std::chrono::duration_cast<std::chrono::milliseconds>(check_time).count();
                        out << check_time_ms / 1000 << "." << (check_time_ms % 1000) << " s -> ";

                        // We reach a fixed point and cannot merge more partitions
                        if (res != cxxsat::Solver::state_t::STATE_SAT)
                        {
                            out << " UNSAT" << std::endl;
                            break;
                        }

                        out << " SAT " << std::endl;

                        // Look for faulty partitions to be merged
                        std::vector<std::vector<uint32_t>> to_be_merged;

                        to_be_merged.emplace_back();
                        auto& faulty_indexes_next = to_be_merged.back();

                        // Show comb gates initially faulty
                        {
                            for (uint32_t cycle = 0; cycle < comb_faults.size(); cycle++)
                            {
                                std::vector<signal_id_t> faulty_sig_comb;
                                for (const auto& fault : comb_faults.at(cycle))
                                {
                                    if (cxxsat::solver->value(fault.second.f0))
                                    {
                                        faulty_sig_comb.push_back(fault.first);
                                    }
                                }
                                assert(faulty_sig_comb.size() <= k_f_comb);

                                out << "  - Faulty comb gates at clock cycle " << cycle << ": ";
                                for (const signal_id_t sig : faulty_sig_comb)
                                {
                                    const VerilogId sig_id = circuit->bit_name(sig);
                                    if (CONF.enumerate_exploitable) {   
                                        // if ((sig_id.name().rfind("check1", 0) != 0) && 
                                        //     (sig_id.name().rfind("red_mcoutputinst", 0) != 0)) continue;
                                        enumerate_comb_faults.emplace(sig);
                                    }
                                    out << static_cast<uint32_t>(sig) << " (" << sig_id.name() << ") ";
                                }
                                out << std::endl;
                            }
                        }

                        // Show partitions initially faulted
                        std::vector<uint32_t> faulty_indexes_initial;
                        {
                            for (uint32_t part_idx = 0; part_idx < partitions_diff.at(0).size();
                                part_idx++)
                            {
                                const var_t& s = partitions_diff.at(0).at(part_idx);
                                if (cxxsat::solver->value(s))
                                {
                                    faulty_indexes_initial.push_back(part_idx);
                                }
                            }
                            assert(faulty_indexes_initial.size() <= k_f_part);
                            out << "  - Faulty partitions (initial): ";
                            for (uint32_t idx : faulty_indexes_initial)
                            {
                                out << idx << " ( ";
                                for (const auto r : partitions.at(idx))
                                {
                                    out << static_cast<uint32_t>(r) << " ";
                                }
                                out << ") ";
                            }
                            out << std::endl;
                        }

                        // Find all violating partitions in next state
                        {
                            for (uint32_t part_idx = 0; part_idx < partitions_diff.at(1).size();
                                part_idx++)
                            {
                                const var_t& s = partitions_diff.at(1).at(part_idx);
                                if (cxxsat::solver->value(s)) { faulty_indexes_next.push_back(part_idx); }
                            }

                            out << "  - Faulty partitions (next): ";
                            for (uint32_t idx : faulty_indexes_next)
                            {
                                out << idx << " ( ";
                                for (const auto r : partitions.at(idx))
                                {
                                    out << static_cast<uint32_t>(r) << " ";
                                }
                                out << ") ";
                            }
                            out << std::endl;
                            assert(faulty_indexes_next.size() > k_faults);
                        }

                        if (CONF.dump_vcd)
                        {
                            std::string fname = CONF.dump_path + "/k-partitions-";
                            fname += time_str;
                            fname += "-" + std::to_string(solver_iter) + ".vcd";
                            dump_vcd(fname, *circuit, golden_trace, faulty_trace);
                            write_gtkw_savefile(faulty_indexes_initial, to_be_merged.back(),
                                                partitions, *circuit, fname);
                        }


                        ///////////////////     Merge strategy     /////////////////
                        // try to merge from best found to worst, while ignoring
                        // everything that is made impossible

                        if (!CONF.enumerate_exploitable) {
                            std::set<uint32_t> removed_next;
                            for (auto it = to_be_merged.rbegin(); it != to_be_merged.rend(); it++)
                            {
                                const auto& faulty_indexes_next = *it;
                                bool all_present = true;
                                for (uint32_t idx : faulty_indexes_next)
                                {
                                    if (removed_next.find(idx) != removed_next.end())
                                    {
                                        all_present = false;
                                        break;
                                    }
                                }
                                if (!all_present) continue;
                                removed_next.insert(faulty_indexes_next.begin(), faulty_indexes_next.end());

                                // merge random faulty partitions
                                double merged_size = (double)faulty_indexes_next.size() / k_faults;
                                double next_bucket = 0;
                                std::vector<std::vector<uint32_t>> merged_indexes;
                                std::vector<uint32_t> index_copies(faulty_indexes_next.begin(),
                                                                faulty_indexes_next.end());
                                for (uint32_t fi = 0; fi < faulty_indexes_next.size(); fi++)
                                {
                                    assert(index_copies.size() == faulty_indexes_next.size() - fi);
                                    if ((double)fi >= next_bucket)
                                    {
                                        assert(merged_indexes.empty() || !merged_indexes.back().empty());
                                        merged_indexes.emplace_back();
                                        next_bucket += merged_size;
                                        assert(merged_indexes.size() <= k_faults);
                                    }
                                    uint32_t chosen_idx_idx = (uint64_t)rand() % index_copies.size();
                                    merged_indexes.back().push_back(index_copies.at(chosen_idx_idx));
                                    index_copies.erase(index_copies.begin() + chosen_idx_idx);
                                }
                                assert(index_copies.empty());

                                // insert new partitions and diff vars according to merged_indexes
                                for (const auto& to_merge : merged_indexes)
                                {
                                    std::unordered_set<signal_id_t> merged;
                                    std::vector<var_t> diffs0;
                                    std::vector<var_t> diffs1;
                                    out << "  Merge together : ";
                                    for (uint32_t fi : to_merge)
                                    {
                                        out << fi << " ";
                                        merged.insert(partitions.at(fi).begin(), 
                                                    partitions.at(fi).end());
                                        diffs0.push_back(partitions_diff.at(0).at(fi));
                                        diffs1.push_back(partitions_diff.at(1).at(fi));
                                    }
                                    out << std::endl;

                                    partitions.push_back(merged);
                                    partitions_diff.at(0).push_back(cxxsat::solver->make_or(diffs0));
                                    partitions_diff.at(1).push_back(cxxsat::solver->make_or(diffs1));
                                }
                            }

                            // remove all the partitions that have now been merged
                            // this works because the removed_next are sorted upwards
                            uint32_t num_removed = 0;
                            uint32_t last_idx = -1U;
                            for (uint32_t fi : removed_next)
                            {
                                assert((last_idx == -1U) || (fi > last_idx));
                                partitions.erase(partitions.begin() + fi - num_removed);
                                partitions_diff.at(0).erase(partitions_diff.at(0).begin() + fi - num_removed);
                                partitions_diff.at(1).erase(partitions_diff.at(1).begin() + fi - num_removed);
                                num_removed += 1;
                                last_idx = fi;
                            }

                            out << "  Merged: " << removed_next.size()
                                << ", Remaining: " << partitions.size() << std::endl;
                        }

                        out << partition_info(*circuit, partitions, CONF.interesting_names).str();
                    }

                    // solver has returned UNSAT
                    out << "  Partitioning finished with " << partitions.size()
                        << " partitions." << std::endl;

                    if (CONF.dump_partitioning) {
                        std::string part_output_file = CONF.dump_path + "/partitioning-";
                        part_output_file += std::to_string(solver_iter) + ".json";

                        std::ofstream pout(part_output_file);
                        nlohmann::json j;
                        
                        for (uint32_t part_idx = 0; part_idx < partitions.size(); part_idx++) {
                            j[std::to_string(part_idx)] = partitions.at(part_idx);
                        }

                        out << "  Write partitioning in file `" << part_output_file << "`" << std::endl;
                        pout << j;
                        pout.close();
                    }
                }
            }
        }

        const auto end_proc1{std::chrono::steady_clock::now()};
        uint32_t proc1_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_proc1 - start_proc1).count();
        out << "=> Procedure 1 verification time: " << proc1_time_ms / 1000;
        out << "." << (proc1_time_ms % 1000) << " s" << std::endl;

        delete cxxsat::solver;
    }

    ///////////////////////////////////////////////////////////////////////////
    //      Procedure 2 -- Check output integrity
    ///////////////////////////////////////////////////////////////////////////
    
    if (CONF.procedure != PROC_1) {

        // Print banner
        out << std::endl << std::string(80, '*') << std::endl;
        out << std::string(20, ' ') << "Procedure 2 -- Check output integrity";
        out << std::endl << std::string(80, '*') << std::endl;
        

        ////////////////////////////////////////////////////////////////////////////
        //      Unroll the circuit `DELAY` times
        ////////////////////////////////////////////////////////////////////////////
        // - Unroll the golden/faulty execution traces
        // - Faults in registers are possible due to their unconstrained initial state
        // - Faults in combinational logic is inserted while unrolling

        // Initialize golden/faulty traces which are a sequence of circuit states.
        std::vector<std::unordered_map<signal_id_t, var_t>> golden_trace;
        std::vector<std::unordered_map<signal_id_t, var_t>> faulty_trace;
        std::vector<std::unordered_map<signal_id_t, fault_spec_t>> comb_faults;

        cxxsat::solver = new cxxsat::Solver();

        for (uint32_t cycle = 0; cycle <= CONF.delay; cycle++)
        {
            if (cycle == 0)
            {
                unroll_init_with_faults(*circuit, golden_trace, faulty_trace,
                                        faultable_sigs, comb_faults);
                // Assume invariant on golden trace
                assert_invariants_at_step(*circuit, golden_trace, CONF.invariant_list, 0);               
            } else {
                unroll_with_faults(*circuit, golden_trace, faulty_trace,
                                faultable_sigs, comb_faults, alert_signals);
            }

            // Assume no alert at each step 
            assert_no_alert_at_step(*circuit, golden_trace, faulty_trace, CONF.alert_list, cycle);
        }

        assert(comb_faults.size() == 1 + CONF.delay);

        ////////////////////////////////////////////////////////////////////////////
        //      Build vectors of partition differences at cycle 0
        ////////////////////////////////////////////////////////////////////////////
        std::array<std::unordered_map<signal_id_t, var_t>, 1> seq_faults;
        std::array<std::vector<var_t>, 1> partitions_diff;

        const auto& golden_state = golden_trace.at(0);
        const auto& faulty_state = faulty_trace.at(0);
        auto& curr_diff = partitions_diff.at(0);
        auto& curr_fault = seq_faults.at(0);

        for (const auto& partition : partitions)
        {
            std::vector<var_t> current_partition_diff;
            for (const auto& sig : partition)
            {
                const auto& it_g = golden_state.find(sig);
                const auto& it_f = faulty_state.find(sig);
                assert(it_g != golden_state.end());
                assert(it_f != faulty_state.end());
                var_t var = it_g->second ^ it_f->second;
                current_partition_diff.push_back(var);
                curr_fault.emplace(sig, var);
            }
            curr_diff.push_back(cxxsat::solver->make_or(current_partition_diff));
        }

        ////////////////////////////////////////////////////////////////////////////
        //      Build vectors of combinational faults at cycle 0 and 1:d
        ////////////////////////////////////////////////////////////////////////////
        std::array<std::vector<var_t>, 2> comb_fault_vars;
        for (uint32_t cycle = 0; cycle < comb_faults.size(); cycle++)
        {
            for (const auto& m_sig_fault : comb_faults.at(cycle))
                comb_fault_vars.at(cycle ? 1 : 0).push_back(m_sig_fault.second.is_faulted());
        }

        const auto start_proc2{std::chrono::steady_clock::now()};

        // Build set of primary outputs
        std::unordered_set<signal_id_t> primary_outputs;
        for (const signal_id_t& sig_out : circuit->outs())
        {
            if (alert_signals.find(sig_out) == alert_signals.end()) 
                primary_outputs.emplace(sig_out);
        }

        // Build vector of primary output differences at clock cycle 0
        std::vector<var_t> output_diff;
        output_diff.reserve(primary_outputs.size());
        for (const signal_id_t& sig_out : primary_outputs)
        {
            const auto& it_g = golden_state.find(sig_out);
            const auto& it_f = faulty_state.find(sig_out);
            assert(it_g != golden_state.end());
            assert(it_f != faulty_state.end());
            var_t var = it_g->second ^ it_f->second;
            output_diff.push_back(var);
        }

        // Data structure to enumerate exploitable partitions/combinational faults
        std::unordered_set<signal_id_t> enumerate_comb_faults;
        std::unordered_set<uint32_t> enumerate_faulty_partitions;

        ////////////////////////////////////////////////////////////////////////////
        //      OPTIMIZATIONS
        ////////////////////////////////////////////////////////////////////////////
        
        // Allow faulty partitions only if connected to circuit primary outputs
        uint32_t part_fault_count = 0;
        for (uint32_t part_idx = 0; part_idx < partitions.size(); part_idx++)
        {
            // Build a set of adjacent registers to the current partition
            std::unordered_set<signal_id_t> conn_outs;
            for (const signal_id_t& sig : partitions.at(part_idx)) {
                const auto& set = circuit->get_conn_outs(sig);
                conn_outs.insert(set->begin(), set->end());
            }

            // Iterator over conn_outs to look for primary output
            auto it = conn_outs.begin();
            for (it; it != conn_outs.end(); it++) {
                if (primary_outputs.find(*it) != primary_outputs.end()) break;
            }

            if (it == conn_outs.end()) {
                cxxsat::solver->add_clause(!partitions_diff.at(0).at(part_idx));
                part_fault_count++;
            }
        }
        out << "  Optimize " << part_fault_count << " faults in partitions" << std::endl;


        // Allow comb faults only if connected to circuit primary outputs
        uint32_t comb_fault_count = 0;
        for (const auto& sig_fault : comb_faults.at(0)) {

            // Get connected outputs to the current signal
            const std::unordered_set<signal_id_t>& conn_outs = *circuit->get_conn_outs(sig_fault.first);

            // Iterator over conn_outs to look for primary output
            auto it = conn_outs.begin();
            for (it; it != conn_outs.end(); it++) {
                if (primary_outputs.find(*it) != primary_outputs.end()) break;
            }

            if (it == conn_outs.end()) {
                cxxsat::solver->add_clause(!sig_fault.second.is_faulted());
                comb_fault_count++;
            }
        }
        out << "  Optimize " << comb_fault_count << " faults in comb logic" << std::endl;



        for (uint32_t k_faults = (CONF.increasing_k) ? 1 : CONF.k; k_faults <= CONF.k; k_faults++)
        {
            // Set the iteration loop of comb faults to 0 if needed
            uint32_t max_k_f_comb = (CONF.f_gates == SEQ) ? 0 : k_faults;
            for (uint32_t k_f_comb = 0; k_f_comb <= max_k_f_comb; k_f_comb++)
            {
                uint32_t k_f_part = k_faults - k_f_comb;

                out << std::string(80, '-') << std::endl;
                out << "Check output integrity for " << k_f_part << "/" << partitions.size()
                    << " faulty partitions," << std::endl;
                out << k_f_comb << "/" << comb_fault_vars.at(0).size() + comb_fault_vars.at(1).size()
                    << " combinational faults" << std::endl;
                out << std::string(80, '-') << std::endl;

                cxxsat::Solver::state_t res = cxxsat::Solver::state_t::STATE_SAT;

                ///////////////////     ASSUMPTIONS     ///////////////////////
                std::vector<var_t> total_comb_f_vars(comb_fault_vars.at(0));
                total_comb_f_vars.insert(total_comb_f_vars.end(),
                        comb_fault_vars.at(1).begin(), comb_fault_vars.at(1).end());

                // Initially, at most `k_f_comb` comb faults
                var_t at_most_k_f_comb = cxxsat::solver->make_at_most(total_comb_f_vars, k_f_comb);

                // Initially, at most `k_f_part` partitions faulted
                var_t at_most_k_f_part = cxxsat::solver->make_at_most(partitions_diff.at(0), k_f_part);

                // At least on faulty primary output
                var_t at_most_1_f_output = cxxsat::solver->make_or(output_diff);
                for (;solver_iter<MAX_ITER; solver_iter++)
                {
                    // Assumptions
                    cxxsat::solver->assume(at_most_k_f_comb);
                    cxxsat::solver->assume(at_most_k_f_part);
                    cxxsat::solver->assume(at_most_1_f_output);

                    // Assume no comb faults that we already enumerated
                    out << std::endl << "Enumerate exploitable faults: ";
                    for (const signal_id_t& sig : enumerate_comb_faults)
                    {
                        out << static_cast<uint32_t>(sig) << " ";
                        const auto& f = comb_faults.at(0).find(sig);
                        assert(f != comb_faults.at(0).end());
                        cxxsat::solver->add_clause(!f->second.is_faulted());
                    }
                    out << std::endl;

                    // Assume no faulty partitions that we already enumerated
                    out << "Enumerate exploitable partitions: ";
                    for (const uint32_t& idx : enumerate_faulty_partitions)
                    {
                        out << idx << " ";
                        const var_t& v = partitions_diff.at(0).at(idx);
                        cxxsat::solver->add_clause(!v);
                    }
                    out << std::endl;

                    out << std::endl << "  Running solver " << solver_iter << ": " << std::flush;

                    const auto start_check{std::chrono::steady_clock::now()};
                    res = cxxsat::solver->check();
                    const auto end_check{std::chrono::steady_clock::now()};
                    
                    const std::chrono::duration check_time = end_check - start_check;
                    uint32_t check_time_ms =
                        std::chrono::duration_cast<std::chrono::milliseconds>(check_time).count();

                    if (res == cxxsat::Solver::state_t::STATE_UNSAT)
                    {
                        out << "UNSAT " << check_time_ms / 1000 << "." << (check_time_ms % 1000)
                            << " s" << std::endl;
                        break;
                    }

                    out << "SAT " << check_time_ms / 1000 << "."
                        << (check_time_ms % 1000) << " s" << std::endl;

                    // Show comb gates initially faulty
                    {
                        for (uint32_t cycle = 0; cycle < comb_faults.size(); cycle++)
                        {
                            std::vector<signal_id_t> faulty_sig_comb;
                            for (const auto& fault : comb_faults.at(cycle))
                            {
                                if (cxxsat::solver->value(fault.second.f0))
                                {
                                    faulty_sig_comb.push_back(fault.first);
                                    enumerate_comb_faults.emplace(fault.first);
                                }
                            }
                            assert(faulty_sig_comb.size() <= k_f_comb);

                            out << "Faulty comb gates at clock cycle " << cycle << ": ";
                            for (const signal_id_t sig : faulty_sig_comb)
                            {
                                out << static_cast<uint32_t>(sig) << " ";
                            }
                            out << std::endl;
                        }
                    }

                    // Show partitions initially faulted
                    std::vector<uint32_t> faulty_indexes_initial;
                    {
                        const auto& initial_part_diff = partitions_diff.at(0);

                        for (uint32_t part_idx = 0; part_idx < initial_part_diff.size(); part_idx++)
                        {
                            const var_t& s = initial_part_diff.at(part_idx);
                            if (cxxsat::solver->value(s))
                            {
                                faulty_indexes_initial.push_back(part_idx);
                                enumerate_faulty_partitions.emplace(part_idx);
                            }
                        }
                        assert(faulty_indexes_initial.size() <= k_f_part);

                        out << "Faulty partitions (initial): ";
                        for (uint32_t idx : faulty_indexes_initial)
                        {
                            out << idx << " ( ";
                            for (const auto r : partitions.at(idx))
                            { out << static_cast<uint32_t>(r) << " "; }
                            out << ") ";
                        }
                        out << std::endl;
                    }

                    // Show corrupted outputs
                    {
                        out << "Corrupted outputs: ";
                        for (const signal_id_t& sig_out : circuit->outs())
                        {
                            const auto& it_g = golden_state.find(sig_out);
                            const auto& it_f = faulty_state.find(sig_out);
                            assert(it_g != golden_state.end());
                            assert(it_f != faulty_state.end());
                            if (cxxsat::solver->value(it_g->second) != cxxsat::solver->value(it_f->second))
                                out << static_cast<uint32_t>(sig_out) << " ";
                        }
                        out << std::endl;
                    }

                    if (CONF.dump_vcd)
                    {
                        std::string fname = CONF.dump_path + "/k-partitions-output-";
                        fname += time_str;
                        fname += ".vcd";
                        dump_vcd(fname, *circuit, golden_trace, faulty_trace);
                    }

                }                        
            }
        }

        const auto end_proc2{std::chrono::steady_clock::now()};
        uint32_t proc2_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_proc2 - start_proc2).count();
        out << "=> Procedure 2 verification time: " << proc2_time_ms / 1000;
        out << "." << (proc2_time_ms % 1000) << " s" << std::endl;

        delete cxxsat::solver;
    }
    out.close();
    delete circuit;
}

int main(int argc, char* argv[])
{
    std::string config_name = "default";

    if (argc == 2)
        config_name = argv[1];

    check_k_fault_resistant_partitioning(config_name);
    return 0;
}
