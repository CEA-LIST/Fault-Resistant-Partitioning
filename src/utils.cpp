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

#include "utils.h"

using json = nlohmann::json;

std::string replace_all(const std::string& s, const std::string& x, const std::string& y) 
{ 
    std::string input(s);
    size_t pos = input.find(x);
    while (pos != std::string::npos) {
        input.replace(pos, x.size(), y); 
        pos = input.find(x, pos + y.size()); 
    }  
    return input; 
}

void dump_vcd(const std::string& file_name, const Circuit& circ,
              const std::vector<std::unordered_map<signal_id_t, var_t>>& trace_g,
              const std::vector<std::unordered_map<signal_id_t, var_t>>& trace_f,
              const std::string& option)
{
    std::ofstream out(file_name);

    assert(trace_f.size() == trace_g.size());

    // Print the current time
    {
        auto now_timepoint = std::chrono::system_clock::now();
        std::time_t curr_time = std::chrono::system_clock::to_time_t(now_timepoint);
        out << "$date" << std::endl;
        out << "\t" << std::ctime(&curr_time) << std::endl;
        out << "$end" << std::endl;
    }

    // Print the generator version
    {
        out << "$version" << std::endl;
        out << "\t" << "FI Verification Tool v0.01" << std::endl;
        out << "$end" << std::endl;
    }

    // Print the timescale
    {
        out << "$timescale" << std::endl;
        out << "\t" << "1ps" << std::endl;
        out << "$end" << std::endl;
    }

    // Get all named signals
    std::unordered_map<signal_id_t, const std::string> signals_in_vcd;
    using scope_entry_t = std::tuple<const char*, std::string, std::string, uint32_t>;
    std::vector<scope_entry_t> scope_data;
    for (auto& it : circ.nets())
    {
        // Exclude symbols if they are not reg, in, out or clock
        if (option == "regs") {
            if (!circ.regs().count(it.second.at(0)) &&
                // !circ.ins().count(it.second.at(0))  &&
                // !circ.outs().count(it.second.at(0)) &&
                circ.clock() != it.second.at(0)) continue;
        }
        // Replace all occurences of ':' which are not allowed in vcd format
        std::string name = replace_all(it.first, ":", "_");
        if (name.find('$') != std::string::npos)
            name = "\\" + name;

        const std::vector<signal_id_t> bits = it.second;
        for (int pos = bits.size() - 1; pos >= 0; pos--)
        {
            const signal_id_t sig_id = bits[pos];
            std::string sig_str = vcd_identifier(sig_id);
            signals_in_vcd.emplace(sig_id, sig_str);
            const char* type = "wire 1";
            scope_data.emplace_back(type, sig_str, name, pos);
        }
    }

    if (circ.clock() != signal_id_t::S_0)
        signals_in_vcd.erase(circ.clock());

    std::vector<std::pair<std::string, std::string>> module_shorthand =
            {{"golden", "g"}, {"faulty", "f"}, {"diff", "d"}};

    for (auto& it : module_shorthand)
    {
        out << "$scope module " << it.first << " $end" << std::endl;
        for (const auto& entry : scope_data)
        {
            out << "\t$var " << std::get<0>(entry) << " " << it.second << std::get<1>(entry);
            out << " " << std::get<2>(entry) << "[" << std::get<3>(entry) << "]" << " $end" << std::endl;
        }
        out << "$upscope $end" << std::endl;
    }
    out << "$enddefinitions $end" << std::endl;

    if (trace_g.empty())
    {
        out.close();
        return;
    }

    // Dump the first cycle
    auto prev_ptr_g = trace_g.cbegin();
    auto curr_ptr_g = trace_g.cbegin();
    auto prev_ptr_f = trace_f.cbegin();
    auto curr_ptr_f = trace_f.cbegin();

    uint32_t curr_tick = 0;

    while(curr_ptr_g != trace_g.end() && curr_ptr_f != trace_f.end())
    {
        out << "#" << curr_tick << std::endl;
        if (curr_tick == 0) out << "$dumpvars" << std::endl;

        const std::unordered_map<signal_id_t, var_t>& curr_map_g = *curr_ptr_g;
        const std::unordered_map<signal_id_t, var_t>& prev_map_g = *prev_ptr_g;

        const std::unordered_map<signal_id_t, var_t>& curr_map_f = *curr_ptr_f;
        const std::unordered_map<signal_id_t, var_t>& prev_map_f = *prev_ptr_f;

        if (circ.clock() != signal_id_t::S_0)
        {
            out << "b1 g" << vcd_identifier(circ.clock()) << std::endl;
            out << "b1 f" << vcd_identifier(circ.clock()) << std::endl;
            out << "b1 d" << vcd_identifier(circ.clock()) << std::endl;
        }

        for (const auto& it: signals_in_vcd)
        {
            auto curr_find_it_g = curr_map_g.find(it.first);
            auto prev_find_it_g = prev_map_g.find(it.first);
            auto curr_find_it_f = curr_map_f.find(it.first);
            auto prev_find_it_f = prev_map_f.find(it.first);

            assert((curr_find_it_g != curr_map_g.end()) ==
                   (curr_find_it_f != curr_map_f.end()));
            assert((prev_find_it_g != prev_map_g.end()) ==
                   (prev_find_it_f != prev_map_f.end()));

            const std::string vcd_id = it.second;
            if (curr_tick == 0)
            {
                if (curr_find_it_g != curr_map_g.end())
                {
                    bool val_g = cxxsat::solver->value(curr_find_it_g->second);
                    bool val_f = cxxsat::solver->value(curr_find_it_f->second);

                    out << "b" << val_g << " g" << vcd_id << std::endl;
                    out << "b" << val_f << " f" << vcd_id << std::endl;
                    show_diff(out, vcd_id, val_g, val_f);
                }
                else
                {
                    out << "bz g" << vcd_id << std::endl;
                    out << "bz f" << vcd_id << std::endl;
                    out << "bz d" << vcd_id << std::endl;
                }
            }
            else if (curr_find_it_g != curr_map_g.end())
            {
                bool curr_val_g = cxxsat::solver->value(curr_find_it_g->second);
                bool curr_val_f = cxxsat::solver->value(curr_find_it_f->second);

                if (prev_find_it_g == prev_map_g.end() ||
                    curr_val_g != cxxsat::solver->value(prev_find_it_g->second))
                { out << "b" << curr_val_g << " g" << vcd_id << std::endl; }
                if (prev_find_it_f == prev_map_f.end() ||
                    curr_val_f != cxxsat::solver->value(prev_find_it_f->second))
                { out << "b" << curr_val_f << " f" << vcd_id << std::endl; }

                if (prev_find_it_g == prev_map_g.end() && prev_find_it_f == prev_map_f.end())
                {
                    assert(false);
                    show_diff(out, vcd_id, curr_val_g, curr_val_f);
                }
                else
                {
                    bool prev_val_g = cxxsat::solver->value(prev_find_it_g->second);
                    bool prev_val_f = cxxsat::solver->value(prev_find_it_f->second);

                    if ((curr_val_g != prev_val_g) || (curr_val_f != prev_val_f))
                    {
                        show_diff(out, vcd_id, curr_val_g, curr_val_f);
                    }
                }

            }
        }

        if (curr_tick == 0) out << "$end" << std::endl;

        if (circ.clock() != signal_id_t::S_0)
        {
            out << "#" << curr_tick + 500 << std::endl;
            out << "b0 g" << vcd_identifier(circ.clock()) << std::endl;
            out << "b0 f" << vcd_identifier(circ.clock()) << std::endl;
            out << "b0 d" << vcd_identifier(circ.clock()) << std::endl;
        }

        curr_tick += 1000;

        prev_ptr_g = curr_ptr_g;
        curr_ptr_g++;
        prev_ptr_f = curr_ptr_f;
        curr_ptr_f++;
    }

    out << "#" << curr_tick << std::endl;
}

void write_gtkw_savefile(const std::vector<uint32_t>& faulty_initial,
                         const std::vector<uint32_t>& faulty_next,
                         const std::vector<std::unordered_set<signal_id_t>>& partitions,
                         const Circuit& circuit, const std::string& dumpfile)
{
    std::string savefile = dumpfile.substr(0, dumpfile.find(".vcd")) + ".gtkw";
    std::ofstream out(savefile);
    out << "[*] Fault analysis result" << std::endl;
    std::string dumfile_basename = dumpfile.substr(dumpfile.find_last_of("/") + 1);
    out << "[dumpfile] \"" << dumfile_basename << "\"" << std::endl;

    const char* open_group_magic = "@800200";
    const char* close_group_magic = "@1000200";
    const char* display_binary_magic = "@8";
    for (uint32_t f_idx : faulty_initial)
    {
        const std::unordered_set<signal_id_t>& partition = partitions.at(f_idx);
        out << open_group_magic << std::endl << "-initial faulty " << f_idx << std::endl;
        out << display_binary_magic << std::endl;
        for (signal_id_t sig : partition)
        {
            const VerilogId sig_id = circuit.bit_name(sig);
            out << "diff.\\" << replace_all(sig_id.name(), ":", "_") << "[" << sig_id.pos() << "]" << std::endl;
        }
        out << close_group_magic << std::endl << "-initial faulty " << f_idx << std::endl;
    }

    for (uint32_t f_idx : faulty_next)
    {
        const std::unordered_set<signal_id_t>& partition = partitions.at(f_idx);
        out << open_group_magic << std::endl << "-next faulty " << f_idx << std::endl;
        out << display_binary_magic << std::endl;
        for (signal_id_t sig : partition)
        {
            const VerilogId sig_id = circuit.bit_name(sig);
            out << "diff.\\" << replace_all(sig_id.name(), ":", "_") << "[" << sig_id.pos() << "]" << std::endl;
        }
        out << close_group_magic << std::endl << "-next faulty " << f_idx << std::endl;
    }
    out.close();
}

std::stringstream partition_info(const Circuit& circuit,
                          const std::vector<std::unordered_set<signal_id_t>>& partitions,
                          const std::vector<std::string>& interesting_names)
{
    std::stringstream ss;
    ss << "******* Partition info ********" << std::endl;
    ss << "Number of partitions: " << partitions.size() << std::endl;
    ss << "Largest partitions: ";

    std::unordered_set<uint32_t> large;
    std::array<uint32_t, 10> large_idxs = {0};
    for (int i = 0; i < std::min(size_t(10), partitions.size()); i++)
    {
        uint32_t max_idx = 0;
        while (large.find(max_idx) != large.end()) { max_idx++; }
        for (uint32_t p_idx = 1; p_idx < partitions.size(); p_idx++)
        {
            if (large.find(p_idx) != large.end()) continue;
            if (partitions.at(p_idx).size() > partitions.at(max_idx).size()) { max_idx = p_idx; }
        }
        large.insert(max_idx);
        large_idxs.at(i) = max_idx;
        ss << "(" << max_idx << ": " << partitions.at(max_idx).size() << ") ";
    }
    ss << std::endl;

    if (!interesting_names.empty()) {
        for (int i = 0; i < 4; i++)
        {
            ss << "Contents of " << large_idxs.at(i) << ": ";
            for (const std::string& name : interesting_names)
            {
                uint32_t num_found = 0;
                for (signal_id_t sig : partitions.at(large_idxs.at(i)))
                {
                    num_found +=
                        (uint32_t)(circuit.bit_name(sig).display().find(name) != std::string::npos);
                }
                ss << "(" << name << ": " << num_found << ") ";
            }
            ss << std::endl;
        }
    }
    return ss;
}

void init_constants(std::unordered_map<signal_id_t, var_t>& state)
{
    state.emplace(signal_id_t::S_0, var_t::ZERO);
    state.emplace(signal_id_t::S_1, var_t::ONE);
    state.emplace(signal_id_t::S_X, var_t::ZERO);
    state.emplace(signal_id_t::S_Z, var_t::ZERO);
}

void unroll_with_faults(const Circuit& circuit,
                        std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                        std::vector<std::unordered_map<signal_id_t, var_t>>& faulty_trace,
                        const std::unordered_set<signal_id_t>& f_sigs,
                        std::vector<std::unordered_map<signal_id_t, fault_spec_t>>& faults,
                        const std::unordered_set<signal_id_t>& alert_signals)
{
    assert(golden_trace.size() == faulty_trace.size());
    assert(golden_trace.size() == faults.size());
    const uint32_t num_steps = golden_trace.size();

    golden_trace.emplace_back();
    faulty_trace.emplace_back();
    faults.emplace_back();
    
    std::unordered_map<signal_id_t, var_t>& golden_state = golden_trace.back();
    std::unordered_map<signal_id_t, var_t>& faulty_state = faulty_trace.back();
    std::unordered_map<signal_id_t, fault_spec_t>& current_faults = faults.back();
    
    init_constants(golden_state);
    init_constants(faulty_state);

    // Declare golden symbols for inputs
    for (signal_id_t sig : circuit.ins()) {
        golden_state.emplace(sig, cxxsat::solver->new_var());
    }

    // Create symbols as faulty copies for inputs belonging to f_sigs
    for (signal_id_t sig: circuit.ins())
    {
        if (f_sigs.find(sig) != f_sigs.end()) {
            fault_spec_t f;
            current_faults.emplace(sig, f);
            faulty_state.emplace(sig, f.induce_fault(golden_state.at(sig)));
        } else {
            faulty_state.emplace(sig, golden_state.at(sig));
        }
    }

    const std::unordered_map<signal_id_t, var_t>& prev_golden_state = golden_trace.at(num_steps - 1);
    const std::unordered_map<signal_id_t, var_t>& prev_faulty_state = faulty_trace.at(num_steps - 1);

    for (const Cell* cell : circuit.cells())
    {
        cell->eval<var_t, var_t&, cxxsat::from_bool>(prev_golden_state, golden_state);
        cell->eval<var_t, var_t&, cxxsat::from_bool>(prev_faulty_state, faulty_state);

        if (is_register(cell->type())) continue;

        const Ports& ports = cell->ports();
        assert(&(ports.m_unr.m_out_y) == &(ports.m_bin.m_out_y));
        assert(&(ports.m_unr.m_out_y) == &(ports.m_mux.m_out_y));
        const signal_id_t& cell_out = ports.m_unr.m_out_y;
        
        // Can be faulted if belongs to f_sigs
        if (f_sigs.find(cell_out) == f_sigs.end()) continue;

        // Can be faulted if cell output is combinationally connected to an alert
        const std::unordered_set<signal_id_t>& conn_outs = *circuit.get_conn_outs(cell_out);
        for (const signal_id_t& out : conn_outs) {
            if (alert_signals.find(out) != alert_signals.end()) {
                fault_spec_t f;
                current_faults.emplace(cell_out, f);
                faulty_state.at(cell_out) = f.induce_fault(faulty_state.at(cell_out));
                break;
            }
        }
    }
}

void unroll_init_with_faults(const Circuit& circuit,
                             std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                             std::vector<std::unordered_map<signal_id_t, var_t>>& faulty_trace,
                             const std::unordered_set<signal_id_t>& f_sigs,
                             std::vector<std::unordered_map<signal_id_t, fault_spec_t>>& faults)
{
    assert(golden_trace.empty());
    assert(faulty_trace.empty());
    assert(faults.empty());

    faulty_trace.emplace_back();
    golden_trace.emplace_back();
    faults.emplace_back();

    std::unordered_map<signal_id_t, var_t>& golden_state = golden_trace.back();
    std::unordered_map<signal_id_t, var_t>& faulty_state = faulty_trace.back();
    std::unordered_map<signal_id_t, fault_spec_t>& current_faults = faults.back();

    init_constants(golden_state);
    init_constants(faulty_state);

    // Declare golden symbols for inputs
    for (signal_id_t sig : circuit.ins())
    { golden_state.emplace(sig, cxxsat::solver->new_var()); }

    // Create symbols as faulty copies for inputs belonging to f_sigs
    for (const signal_id_t sig: circuit.ins())
    {
        if (f_sigs.find(sig) != f_sigs.end()) {
            fault_spec_t f;
            current_faults.emplace(sig, f);
            faulty_state.emplace(sig, f.induce_fault(golden_state.at(sig)));
        } else {
            faulty_state.emplace(sig, golden_state.at(sig));
        }
    }

    // Duplicate regs
    for (const signal_id_t sig : circuit.regs())
    {
        golden_state.emplace(sig, cxxsat::solver->new_var());
        faulty_state.emplace(sig, cxxsat::solver->new_var());
    }

    // Forward the symbols through the wires
    std::unordered_map<signal_id_t, var_t> empty;

    for (const Cell* cell : circuit.cells())
    {
        if (is_register(cell->type())) continue;

        cell->eval<var_t, var_t&, cxxsat::from_bool>(empty, golden_state);
        cell->eval<var_t, var_t&, cxxsat::from_bool>(empty, faulty_state);

        const Ports& ports = cell->ports();
        assert(&(ports.m_unr.m_out_y) == &(ports.m_bin.m_out_y));
        assert(&(ports.m_unr.m_out_y) == &(ports.m_mux.m_out_y));
        const signal_id_t& out_sig = ports.m_bin.m_out_y;

        if (f_sigs.find(out_sig) != f_sigs.end()) {
            fault_spec_t f;
            current_faults.emplace(out_sig, f);
            faulty_state.at(out_sig) = f.induce_fault(faulty_state.at(out_sig));
        }
    }
}

void assert_invariants_at_step(const Circuit& circuit,
                               const std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                               const std::unordered_map<std::string, std::vector<bool>> invariant_list,
                               const uint32_t step)
{
    assert(step < golden_trace.size());
    for (const auto& inv: invariant_list) {

        const std::vector<signal_id_t>& sig = circuit[inv.first];
        const std::vector<bool>& bitvec = inv.second;

        assert(sig.size() == bitvec.size());
        for (uint32_t pos = 0; pos < sig.size(); pos++) {
            var_t symbol = golden_trace.at(step).at(sig.at(pos));
            bool value = bitvec.at(pos);
            cxxsat::solver->add_clause(value ? symbol : (!symbol));
        }
    }
}

void assert_no_alert_at_step(const Circuit& circuit,
                             const std::vector<std::unordered_map<signal_id_t, var_t>>& golden_trace,
                             const std::vector<std::unordered_map<signal_id_t, var_t>>& faulty_trace,
                             const std::unordered_map<std::string, std::vector<bool>>& alert_list,
                             const uint32_t step)
{
    assert(step < golden_trace.size());
    assert(golden_trace.size() == faulty_trace.size());
    const std::unordered_map<signal_id_t, var_t>& golden_state = golden_trace.at(step);
    const std::unordered_map<signal_id_t, var_t>& faulty_state = faulty_trace.at(step);

    for (const auto& alert: alert_list)
    {
        const std::vector<signal_id_t>& sig = circuit[alert.first];
        const std::vector<bool>& bitvec = alert.second;
        assert(sig.size() == bitvec.size());

        std::vector<var_t> out_vars;
        for (uint32_t pos = 0; pos < sig.size(); pos++) {
            bool value = bitvec.at(pos);
            var_t g = golden_state.at(sig.at(pos));
            var_t f = faulty_state.at(sig.at(pos));
            out_vars.push_back(value ? g : (!g));
            out_vars.push_back(value ? f : (!f));
        }
        cxxsat::solver->add_clause(cxxsat::solver->make_and(out_vars));
    }
}

void assume_no_comb_fault_if_not_connected_to_outputs(
                const Circuit& circuit,
                const std::unordered_map<signal_id_t, fault_spec_t>& comb_faults)
{
    uint32_t disable_count = 0;
    for (const auto& f : comb_faults) {
        if (circuit.get_conn_outs(f.first)->empty()) {
            cxxsat::solver->assume(!f.second.is_faulted());
            disable_count++;
        }
    }
    std::cout << "Comb faults connected to outputs: " << comb_faults.size() - disable_count;
    std::cout << " / " << comb_faults.size() << std::endl;
}

const var_t fault_spec_t::induce_fault(const var_t normal)
{
    var_t new_value = cxxsat::solver->new_var();
    // 0 no fault
    cxxsat::solver->add_clause( normal,  f0, -new_value);
    cxxsat::solver->add_clause(-normal,  f0,  new_value);
    // 1 bit flip
    cxxsat::solver->add_clause( normal,  -f0, new_value);
    cxxsat::solver->add_clause(-normal, -f0, -new_value);
    return new_value;
}

std::vector<std::unordered_set<signal_id_t>> init_partitions_from_file(const Circuit& circuit,
                                                                const std::string file_name)
{
    std::ifstream f; f.exceptions(std::ifstream::badbit);
    f.open(file_name);
    std::string data(std::istreambuf_iterator<char>{f}, {});
    f.close();
    auto jdata = json::parse(data);

    std::vector<std::unordered_set<signal_id_t>> partitions;
    partitions.reserve(jdata.size());

    const auto& regs = circuit.regs();
    std::unordered_set<signal_id_t> visited_regs;

    for (const auto& port_data: jdata.items()) {
        const auto& current_part = port_data.value();
        assert(!current_part.empty());

        auto& emplace_it = partitions.emplace_back(std::unordered_set<signal_id_t>());
        for (const auto& sig : current_part) {
            assert(regs.find(sig) != regs.end());
            emplace_it.emplace(sig);
            visited_regs.emplace(sig);
        }
    }
    assert(visited_regs.size() == regs.size());
    return partitions;
}

std::vector<std::unordered_set<signal_id_t>> init_partitions_from_scratch(const Circuit& circuit) {
    std::vector<std::unordered_set<signal_id_t>> partitions;
    for (const signal_id_t reg : circuit.regs())
        { partitions.push_back({reg}); }
    return partitions;
}

std::unordered_set<signal_id_t> compute_faultable_signals(
    const Circuit& circuit,
    const std::vector<std::string>& f_included_prefix,
    const std::vector<std::string>& f_excluded_prefix,
    const std::vector<signal_id_t>& f_excluded_signals,
    const bool exclude_inputs)
{           
    // Compute signals to exlude    
    std::set<signal_id_t> excluded_sigs;
    for (const std::string& excl_prefix : f_excluded_prefix) {
        for (auto & net : circuit.nets()) {
            if (net.first.rfind(excl_prefix, 0) != 0) continue;
            const auto& sigs = net.second;
            excluded_sigs.insert(sigs.begin(), sigs.end());
        }
    }
    // Exclude inputs if required
    if (exclude_inputs) {
        const auto& ins = circuit.ins();
        excluded_sigs.insert(ins.begin(), ins.end());
    }

    // Exclude additional signals
    excluded_sigs.insert(f_excluded_signals.begin(), f_excluded_signals.end());

    // Compute signals to include. Include them all if empty
    std::set<signal_id_t> included_sigs;
    for (const std::string& f_prefix : f_included_prefix) {
        for (auto & net : circuit.nets()) {
            if (net.first.rfind(f_prefix, 0) != 0) continue;
            const auto& sigs = net.second;
            included_sigs.insert(sigs.begin(), sigs.end());
        }
    }
    if (f_included_prefix.empty()) {
        const auto& all_sigs = circuit.sigs();
        included_sigs.insert(all_sigs.begin(), all_sigs.end());
    }

    // Compute set difference and return it
    std::unordered_set<signal_id_t> signals;
    std::set_difference(included_sigs.begin(), included_sigs.end(),
                        excluded_sigs.begin(), excluded_sigs.end(),
                        std::inserter(signals, signals.begin()));
    return signals;
}

std::unordered_set<uint32_t> get_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const uint32_t part_idx)
{
    assert(part_idx < partitions.size());
    const auto& curr_part = partitions.at(part_idx);

    // Build a set of adjacent registers to the current partition
    std::unordered_set<signal_id_t> adjacent_regs;
    for (const signal_id_t& sig : curr_part) {
        const auto& set = circuit.get_conn_regs(sig);
        adjacent_regs.insert(set->begin(), set->end());
    }

    std::unordered_set<uint32_t> conn_part_indexes;
    for (uint32_t idx = 0; idx < partitions.size(); idx++) {
        for (const signal_id_t& sig : partitions.at(idx)) {
            if (adjacent_regs.find(sig) != adjacent_regs.end())
                conn_part_indexes.emplace(idx);
        }
    }

    return conn_part_indexes;
}

std::unordered_set<uint32_t> get_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const signal_id_t& sig)
{
    assert(circuit.regs().find(sig) == circuit.regs().end());

    // Get connected registers to the current signal
    const std::unordered_set<signal_id_t>& adjacent_regs = *circuit.get_conn_regs(sig);

    std::unordered_set<uint32_t> conn_part_indexes;
    for (uint32_t idx = 0; idx < partitions.size(); idx++) {
        for (const signal_id_t& sig : partitions.at(idx)) {
            if (adjacent_regs.find(sig) != adjacent_regs.end())
                conn_part_indexes.emplace(idx);
        }
    }

    return conn_part_indexes;
}

bool at_least_2_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const uint32_t part_idx)
{
    assert(part_idx < partitions.size());
    const auto& curr_part = partitions.at(part_idx);

    // Build a set of adjacent registers to the current partition
    std::unordered_set<signal_id_t> adjacent_regs;
    for (const signal_id_t& sig : curr_part) {
        const auto& set = circuit.get_conn_regs(sig);
        adjacent_regs.insert(set->begin(), set->end());
    }

    if (adjacent_regs.size() <= 1) return false;

    auto it = adjacent_regs.begin();
    const signal_id_t& conn_reg = *it;

    int conn_part_idx = -1;
    for (uint32_t idx = 0; idx < partitions.size(); idx++) {
        if (partitions.at(idx).find(conn_reg) != partitions.at(idx).end())
            conn_part_idx = idx;
    }

    for (++it; it != adjacent_regs.end(); it++) {
        if (partitions.at(conn_part_idx).find(*it) == partitions.at(conn_part_idx).end())
            return true;
    }

    return false;
}

bool at_least_2_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const signal_id_t& sig)
{
    assert(circuit.regs().find(sig) == circuit.regs().end());

    // Get connected registers to the current signal
    const std::unordered_set<signal_id_t>& adjacent_regs = *circuit.get_conn_regs(sig);

    if (adjacent_regs.size() <= 1) return false;

    auto it = adjacent_regs.begin();
    const signal_id_t& conn_reg = *it;

    int conn_part_idx = -1;
    for (uint32_t idx = 0; idx < partitions.size(); idx++) {
        if (partitions.at(idx).find(conn_reg) != partitions.at(idx).end())
            conn_part_idx = idx;
    }

    for (++it; it != adjacent_regs.end(); it++) {
        if (partitions.at(conn_part_idx).find(*it) == partitions.at(conn_part_idx).end())
            return true;
    }

    return false;
}

std::stringstream optim_at_least_2_conn_parts(
    const Circuit& circuit,
    const std::vector<std::unordered_set<signal_id_t>>& partitions,
    const std::unordered_map<signal_id_t, fault_spec_t>& initial_comb_faults,
    const std::vector<var_t>& initial_partitions_diff)
{
    std::stringstream ss;
    // Map register id with partition index
    std::unordered_map<signal_id_t, uint32_t> m_reg_partidx;
    m_reg_partidx.reserve(circuit.regs().size());

    for (uint32_t idx = 0; idx < partitions.size(); idx++) {
        for (const signal_id_t reg : partitions.at(idx)) {
            m_reg_partidx.emplace(reg, idx);
        }
    }

    // No faulted partition if connected to at most 1 partition
    int part_optim_nb = 0;
    for (uint32_t idx = 0; idx < partitions.size(); idx++) {

        // Build a set of adjacent registers to the current partition
        std::unordered_set<signal_id_t> adjacent_regs;
        for (const signal_id_t& sig : partitions.at(idx)) {
            const auto& set = circuit.get_conn_regs(sig);
            adjacent_regs.insert(set->begin(), set->end());
        }

        if (adjacent_regs.size() <= 1) {
            cxxsat::solver->add_clause(!initial_partitions_diff.at(idx));
            part_optim_nb++;
            continue;
        }

        // Create iterator over adjacent_regs
        auto it = adjacent_regs.begin();
        int conn_part_idx = m_reg_partidx.at(*it);
        for (++it; it != adjacent_regs.end(); it++) {
            if (m_reg_partidx.at(*it) != conn_part_idx) break;
        }
        
        if (it == adjacent_regs.end()) {
            cxxsat::solver->add_clause(!initial_partitions_diff.at(idx));
            part_optim_nb++;
        }
    }
    ss << "  Optimize " << part_optim_nb << " faults in partitions" << std::endl;

    // No Comb faults if connected to at most 1 partition
    int comb_optim_nb = 0;
    for (const auto& sig_fault : initial_comb_faults) {
        // Get connected registers to the current signal
        const std::unordered_set<signal_id_t>& adjacent_regs = *circuit.get_conn_regs(sig_fault.first);

        if (adjacent_regs.size() <= 1) {
            cxxsat::solver->add_clause(!sig_fault.second.is_faulted());
            comb_optim_nb++;
            continue;
        }

        // Create iterator over adjacent_regs
        auto it = adjacent_regs.begin();
        int conn_part_idx = m_reg_partidx.at(*it);
        for (++it; it != adjacent_regs.end(); it++) {
            if (m_reg_partidx.at(*it) != conn_part_idx) break;
        }
        
        if (it == adjacent_regs.end()) {
            cxxsat::solver->add_clause(!sig_fault.second.is_faulted());
            comb_optim_nb++;
        }
    }
    ss << "  Optimize " << comb_optim_nb << " faults in comb logic" << std::endl;

    return ss;
}