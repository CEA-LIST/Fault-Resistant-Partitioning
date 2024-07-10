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

#include <iostream>
#include <fstream>

#include "Circuit.h"
#include "json.hpp"
using json = nlohmann::json;

Circuit::Circuit(const std::string& json_file_path, const std::string& top_module_name) :
    m_module_name(top_module_name), m_sig_clock(signal_id_t::S_0)
{
    std::ifstream f; f.exceptions(std::ifstream::badbit);
    f.open(json_file_path);
    std::string data(std::istreambuf_iterator<char>{f}, {});
    f.close();
    auto jdata = json::parse(data);
    const auto& module = jdata.at("modules").at(top_module_name);

    m_signals.insert(signal_id_t::S_0);
    m_signals.insert(signal_id_t::S_1);
    m_signals.insert(signal_id_t::S_X);
    m_signals.insert(signal_id_t::S_Z);

    // Register all module_ports of the circuit
    const auto& module_ports = module.at("ports");
    for (const auto& port_data: module_ports.items())
    {
        const auto& key = port_data.key();
        const auto& value = port_data.value();

        // The port name is the key
        std::string name = key;

        // Determine the direction, make sure it is valid
        std::string direction = value.at("direction");
        if (direction != "input" && direction != "output")
            { throw std::logic_error(ILLEGAL_PORT_DIRECTION); }

        // Determine the bits, make sure it is an array
        const auto& bits = value.at("bits");
        if (!bits.is_array())
            { throw std::logic_error(ILLEGAL_SIGNAL_LIST);}

        // Register signal name with empty signal list
        const auto emplace_it = m_name_bits.emplace(name, std::vector<signal_id_t>());

        // If we fail, it is due to a re-definition of the same name
        if (!emplace_it.second)
            { throw std::logic_error(ILLEGAL_NAME_REDECLARATION); }

        // Convert all the bit indexes into signal ids
        std::vector<signal_id_t>& typed_signals = emplace_it.first->second;
        typed_signals.reserve(bits.size());
        for (const auto& bit_id : bits)
            { typed_signals.push_back(get_signal_any(bit_id)); }

        add_bit_names(name, typed_signals);

        // Fill the port signals and overall known signals with those we found
        std::unordered_set<signal_id_t>& direction_ports = direction == "input" ? m_in_ports : m_out_ports;
        for (const signal_id_t sig : typed_signals)
        {
            direction_ports.insert(sig);
            if (direction == "input") m_signals.insert(sig);
        }
    }

    // Register all module_cells of the circuit
    const auto& module_cells = module.at("cells");
    std::unordered_set<signal_id_t> missing;
    for (const auto& cell_data: module_cells.items())
    {
        const auto& key = cell_data.key();
        const auto& value = cell_data.value();

        const char* name = key.c_str();
        std::string str_type = value.at("type");

        // TODO: do this properly
        if (str_type == "$assert") continue;

        cell_type_t type = cell_type_from_string(str_type);
        if(type == cell_type_t::CELL_NONE)
            { throw std::logic_error(ILLEGAL_CELL_TYPE); }

        const auto& connections = value.at("connections");
        if (is_unary(type))
        {
            signal_id_t a = get_signal_any(connections.at("A").at(0));
            signal_id_t y = get_signal_any(connections.at("Y").at(0));
            if (a == y) throw std::logic_error(ILLEGAL_CELL_CYCLE);

            if(m_signals.find(a) == m_signals.end()) { missing.insert(a); }
            assert(m_signals.find(y) == m_signals.end());
            m_signals.insert(y);
            missing.erase(y);
            m_cells.push_back(new Cell(name, type, UnaryPorts(a, y)));
        }
        else if (is_binary(type))
        {
            signal_id_t a = get_signal_any(connections.at("A").at(0));
            signal_id_t b = get_signal_any(connections.at("B").at(0));
            signal_id_t y = get_signal_any(connections.at("Y").at(0));
            if (a == y) throw std::logic_error(ILLEGAL_CELL_CYCLE);
            if (b == y) throw std::logic_error(ILLEGAL_CELL_CYCLE);

            if(m_signals.find(a) == m_signals.end()) { missing.insert(a); }
            if(m_signals.find(b) == m_signals.end()) { missing.insert(b); }
            assert(m_signals.find(y) == m_signals.end());
            m_signals.insert(y);
            missing.erase(y);
            m_cells.push_back(new Cell(name, type, BinaryPorts(a, b, y)));
        }
        else if (is_multiplexer(type))
        {
            signal_id_t a = get_signal_any(connections.at("A").at(0));
            signal_id_t b = get_signal_any(connections.at("B").at(0));
            signal_id_t s = get_signal_any(connections.at("S").at(0));
            signal_id_t y = get_signal_any(connections.at("Y").at(0));
            if (a == y) throw std::logic_error(ILLEGAL_CELL_CYCLE);
            if (b == y) throw std::logic_error(ILLEGAL_CELL_CYCLE);
            if (s == y) throw std::logic_error(ILLEGAL_CELL_CYCLE);

            if(m_signals.find(a) == m_signals.end()) { missing.insert(a); }
            if(m_signals.find(b) == m_signals.end()) { missing.insert(b); }
            if(m_signals.find(s) == m_signals.end()) { missing.insert(s); }
            assert(m_signals.find(y) == m_signals.end());
            m_signals.insert(y);
            missing.erase(y);
            m_cells.push_back(new Cell(name, type, MultiplexerPorts(a, b, s, y)));
        }
        else if (is_register(type))
        {
            signal_id_t c = get_signal_any(connections.at("C").at(0));
            signal_id_t d = get_signal_any(connections.at("D").at(0));
            signal_id_t q = get_signal_any(connections.at("Q").at(0));
            if (c == q) throw std::logic_error(ILLEGAL_CELL_CYCLE);

            if(m_signals.find(c) == m_signals.end()) { missing.insert(c); }
            if(m_signals.find(d) == m_signals.end()) { missing.insert(d); }
            assert(m_signals.find(q) == m_signals.end());

            Cell* p_cell = nullptr;
            if (!dff_has_enable(type) && !dff_has_reset(type))
            {
                p_cell = new Cell(name, type, DffPorts(c, d, q));
            }
            else if (dff_has_reset(type) && !dff_has_enable(type))
            {
                signal_id_t r = get_signal_any(connections.at("R").at(0));
                if (r == q) throw std::logic_error(ILLEGAL_CELL_CYCLE);
                if(m_signals.find(r) == m_signals.end()) { missing.insert(r); }
                p_cell = new Cell(name, type, DffrPorts(c, d, q, r));
            }
            else if (!dff_has_reset(type) && dff_has_enable(type))
            {
                signal_id_t e = get_signal_any(connections.at("E").at(0));
                if (e == q) throw std::logic_error(ILLEGAL_CELL_CYCLE);
                if(m_signals.find(e) == m_signals.end()) { missing.insert(e); }
                p_cell = new Cell(name, type, DffePorts(c, d, q, e));
            }
            else
            {
                signal_id_t r = get_signal_any(connections.at("R").at(0));
                signal_id_t e = get_signal_any(connections.at("E").at(0));
                if (r == q) throw std::logic_error(ILLEGAL_CELL_CYCLE);
                if (e == q) throw std::logic_error(ILLEGAL_CELL_CYCLE);
                if(m_signals.find(r) == m_signals.end()) { missing.insert(r); }
                if(m_signals.find(e) == m_signals.end()) { missing.insert(e); }
                p_cell = new Cell(name, type, DfferPorts(c, d, q, r, e));
            }
            m_signals.insert(q);
            m_reg_outs.insert(q);
            missing.erase(q);
            m_cells.push_back(p_cell);
        }
    }

    // Check that all outputs are actually defined
    if (!missing.empty()) throw std::logic_error(ILLEGAL_MISSING_SIGNALS);
    for (signal_id_t sig: m_out_ports)
    {
        if (m_signals.find(sig) == m_signals.end())
            { throw std::logic_error(ILLEGAL_MISSING_SIGNALS); }
    }

    // Check that the clock edge matches on all registers
    // Also check that all registers have the same clock
    bool found_pos_edge = false;
    bool found_neg_edge = false;

    for (const Cell* p_cell : m_cells)
    {
        if (!is_register(p_cell->type())) continue;
        const bool clock_trigger = dff_clock_trigger(p_cell->type());
        found_pos_edge |= clock_trigger;
        if (!clock_trigger) std::cout << p_cell->name() << std::endl;
        found_neg_edge |= !clock_trigger;

        if (m_sig_clock == signal_id_t::S_0)
        {
            if (p_cell->m_ports.m_dff.m_in_c == signal_id_t::S_0 ||
                p_cell->m_ports.m_dff.m_in_c == signal_id_t::S_1 ||
                p_cell->m_ports.m_dff.m_in_c == signal_id_t::S_X ||
                p_cell->m_ports.m_dff.m_in_c == signal_id_t::S_Z)
                { throw std::logic_error(ILLEGAL_CLOCK_SIGNAL); }
            m_sig_clock = p_cell->m_ports.m_dff.m_in_c;
        }
        else
        {
            if (p_cell->m_ports.m_dff.m_in_c != m_sig_clock)
                { throw std::logic_error(ILLEGAL_MULTIPLE_CLOCKS); }
        }
        // Technically this could not hold due to compiler shenanigans
        assert((&(p_cell->m_ports.m_dff.m_in_c)) == (&(p_cell->m_ports.m_dffr.m_in_c)));
        assert((&(p_cell->m_ports.m_dff.m_in_c)) == (&(p_cell->m_ports.m_dffe.m_in_c)));
        assert((&(p_cell->m_ports.m_dff.m_in_c)) == (&(p_cell->m_ports.m_dffer.m_in_c)));
    }

    if (found_neg_edge && found_pos_edge)
        { throw std::logic_error(ILLEGAL_CLOCK_EDGE); }

    std::unordered_map<signal_id_t, uint32_t> visited_sig;

    for (auto iter = m_in_ports.begin(); iter != m_in_ports.end(); ++iter) {
        visited_sig.emplace(*iter, visited_sig.size());
    }
    visited_sig.emplace(signal_id_t::S_0, visited_sig.size());
    visited_sig.emplace(signal_id_t::S_1, visited_sig.size());
    visited_sig.emplace(signal_id_t::S_X, visited_sig.size());
    visited_sig.emplace(signal_id_t::S_Z, visited_sig.size());

    std::unordered_set<const Cell*> visited_cell;
    std::vector<const Cell*> cell_order;

    for (const Cell* p_cell : m_cells)
    {
        if (!is_register(p_cell->type())) continue;
        cell_order.push_back(p_cell);
        visited_cell.insert(p_cell);
        visited_sig.emplace(p_cell->m_ports.m_dff.m_out_q, visited_sig.size());
        // Technically this could not hold due to compiler shenanigans
        assert((&(p_cell->m_ports.m_dff.m_out_q)) == (&(p_cell->m_ports.m_dffr.m_out_q)));
        assert((&(p_cell->m_ports.m_dff.m_out_q)) == (&(p_cell->m_ports.m_dffe.m_out_q)));
        assert((&(p_cell->m_ports.m_dff.m_out_q)) == (&(p_cell->m_ports.m_dffer.m_out_q)));
    }

    while (cell_order.size() != m_cells.size())
    {
        for (const Cell* p_cell : m_cells)
        {
            if (visited_cell.find(p_cell) != visited_cell.end()) continue;
            const cell_type_t type = p_cell->type();
            const Ports& ports = p_cell->m_ports;
            assert(!is_register(type));
            #define sig_visited(x) (visited_sig.find(x) != visited_sig.end())
            // Check whether the predecessors are visited, otherwise continue
            if (is_unary(type) && sig_visited(ports.m_unr.m_in_a))
            {
                visited_sig.emplace(ports.m_unr.m_out_y, visited_sig.size());
            }
            else if (is_binary(type) && sig_visited(ports.m_bin.m_in_a)
                                     && sig_visited(ports.m_bin.m_in_b))
            {
                visited_sig.emplace(ports.m_bin.m_out_y, visited_sig.size());
            }
            else if (is_multiplexer(type) && sig_visited(ports.m_mux.m_in_a)
                                          && sig_visited(ports.m_mux.m_in_b)
                                          && sig_visited(ports.m_mux.m_in_s))
            {
                visited_sig.emplace(ports.m_mux.m_out_y, visited_sig.size());
            }
            else
            {
                continue;
            }

            // If predecessors are visited, add the cell
            visited_cell.insert(p_cell);
            cell_order.push_back(p_cell);
        }
    }
    m_cells = cell_order;

    /*
    std::ofstream ofs("/tmp/tmp-order.txt");
    for (const Cell* p_cell : m_cells)
    {
        const cell_type_t type = p_cell->type();
        const Ports& ports = p_cell->m_ports;
        if (is_unary(type))
        {
            ofs << visited_sig[ports.m_unr.m_in_a] << " < " << visited_sig[ports.m_unr.m_out_y] << std::endl;
        }
        else if (is_binary(type))
        {
            ofs << visited_sig[ports.m_bin.m_in_a] << " < " << visited_sig[ports.m_bin.m_out_y] << std::endl;
            ofs << visited_sig[ports.m_bin.m_in_b] << " < " << visited_sig[ports.m_bin.m_out_y] << std::endl;
        }
        else if (is_multiplexer(type))
        {
            ofs << visited_sig[ports.m_mux.m_in_a] << " < " << visited_sig[ports.m_mux.m_out_y] << std::endl;
            ofs << visited_sig[ports.m_mux.m_in_b] << " < " << visited_sig[ports.m_mux.m_out_y] << std::endl;
            ofs << visited_sig[ports.m_mux.m_in_s] << " < " << visited_sig[ports.m_mux.m_out_y] << std::endl;
        }
    }
    ofs.close();
    */
    
    // Extract the remaining names from module_netnames
    const auto& module_netnames = module.at("netnames");
    for (const auto& name_data: module_netnames.items())
    {
        const auto& key = name_data.key();
        const auto& value = name_data.value();

        // The port name is the key
        std::string name = key;

        // Determine the bits, make sure it is an array
        const auto& bits = value.at("bits");
        if (!bits.is_array())
            { throw std::logic_error(ILLEGAL_SIGNAL_LIST);}

        // Convert the bits into signals
        std::vector<signal_id_t> typed_signals;
        typed_signals.reserve(bits.size());
        for (const auto& bit_id : bits)
        { typed_signals.push_back(get_signal_any(bit_id)); }

        // If the name already exists, it is re-defined (possibly first within input ports)
        // Make sure that everything is consistent, otherwise throw an error
        const auto& found_it = m_name_bits.find(name);
        if (found_it != m_name_bits.end())
        {
            const std::vector<signal_id_t>& other = found_it->second;
            if (other.size() != typed_signals.size())
                { throw std::logic_error(ILLEGAL_NAME_REDECLARATION); }
            for (size_t i = 0; i < other.size(); i++)
                if (typed_signals[i] != other[i])
                    { throw std::logic_error(ILLEGAL_NAME_REDECLARATION); }
        }
        else
        {
            // If it was not there in the first place, add it as new
            const auto& emplace_it = m_name_bits.emplace(name, typed_signals);

            add_bit_names(name, typed_signals);

            assert(emplace_it.second);
        }
    }

    m_bit_name.emplace(signal_id_t::S_0, VerilogId("constant 0", 0));
    m_bit_name.emplace(signal_id_t::S_1, VerilogId("constant 1", 0));
    m_bit_name.emplace(signal_id_t::S_X, VerilogId("constant X", 0));
    m_bit_name.emplace(signal_id_t::S_Z, VerilogId("constant Z", 0));
}

/* Extract and create a new circuit from a top_circuit and an subcircuit 
 * interface.
 */
Circuit::Circuit(const Circuit& top_circuit,
                 const std::string& subcircuit_interface_json_file,
                 const std::string& top_module_name) :
    m_module_name(top_module_name), m_sig_clock(signal_id_t::S_0)
{
    std::ifstream f; f.exceptions(std::ifstream::badbit);
    f.open(subcircuit_interface_json_file);
    std::string data(std::istreambuf_iterator<char>{f}, {});
    f.close();
    auto jdata = json::parse(data);
    const auto& module = jdata.at("modules").at(top_module_name);

    m_signals.insert(signal_id_t::S_0);
    m_signals.insert(signal_id_t::S_1);
    m_signals.insert(signal_id_t::S_X);
    m_signals.insert(signal_id_t::S_Z);

    // Register all module_ports of the circuit
    const auto& module_ports = module.at("ports");
    for (const auto& port_data: module_ports.items())
    {
        const auto& key = port_data.key();
        const auto& value = port_data.value();

        // The port name is the key
        std::string name = key;

        // Determine the direction, make sure it is valid
        std::string direction = value.at("direction");
        if (direction != "input" && direction != "output")
            { throw std::logic_error(ILLEGAL_PORT_DIRECTION); }

        // Determine the bits, make sure it is an array
        const auto& bits = value.at("bits");
        if (!bits.is_array())
            { throw std::logic_error(ILLEGAL_SIGNAL_LIST);}

        // Register signal name with empty signal list
        const auto emplace_it = m_name_bits.emplace(name, std::vector<signal_id_t>());

        // If we fail, it is due to a re-definition of the same name
        if (!emplace_it.second)
            { throw std::logic_error(ILLEGAL_NAME_REDECLARATION); }

        // Convert all the bit indexes into signal ids
        std::vector<signal_id_t>& typed_signals = emplace_it.first->second;
        typed_signals.reserve(bits.size());
        for (const auto& bit_id : bits)
            { typed_signals.push_back(get_signal_any(bit_id)); }

        add_bit_names(name, typed_signals);

        // Fill the port signals and overall known signals with those we found
        std::unordered_set<signal_id_t>& direction_ports = 
                                direction == "input" ? m_in_ports : m_out_ports;

        for (const signal_id_t sig : typed_signals)
        {
            direction_ports.insert(sig);
            if (direction == "input") m_signals.insert(sig);
        }
    }

    // Explore top_circuit circuit from the provided interface and copy visited cells
    std::unordered_set<signal_id_t> visited_sigs = m_out_ports;
    std::unordered_set<const Cell*> visited_cells;
    int visited_sigs_size = 0;

    #define belongs_to(x,y) (y.find(x) != y.end())

    while (visited_sigs.size() != visited_sigs_size)
    {
        visited_sigs_size = visited_sigs.size();
        for (auto it = top_circuit.cells().rbegin(); it != top_circuit.cells().rend(); it++)
        {
            const Cell* p_cell = *it;
            if (visited_cells.find(p_cell) != visited_cells.end()) continue;
            const cell_type_t type = p_cell->type();
            const Ports& ports = p_cell->m_ports;
            std::unordered_set<signal_id_t> in_ports;

            assert(&(ports.m_unr.m_out_y) == &(ports.m_bin.m_out_y));
            assert(&(ports.m_unr.m_out_y) == &(ports.m_mux.m_out_y));
            assert(&(ports.m_unr.m_out_y) == &(ports.m_dff.m_out_q));
            const signal_id_t& out_sig = ports.m_unr.m_out_y;

            // Continue if current cell is not connected to visited_sigs
            if (!belongs_to(out_sig, visited_sigs)) continue;
            // Continue if current cell is connected to a subcircuit input
            if (belongs_to(out_sig, m_in_ports)) continue;

            // Collect in ports from connected cells
            if (is_unary(type))
            {
                in_ports.emplace(ports.m_unr.m_in_a);
            }
            else if (is_binary(type))
            {
                in_ports.emplace(ports.m_bin.m_in_a);
                in_ports.emplace(ports.m_bin.m_in_b);
            }
            else if (is_multiplexer(type))
            {
                in_ports.emplace(ports.m_mux.m_in_a);
                in_ports.emplace(ports.m_mux.m_in_b);
                in_ports.emplace(ports.m_mux.m_in_s);
            }
            else
            {
                assert(is_register(type));
                in_ports.emplace(ports.m_dff.m_in_d);
                in_ports.emplace(ports.m_dff.m_in_c);
                if (dff_has_enable(type))
                {
                    bool e_only = test_is_reg_with_enable(type);
                    in_ports.emplace(e_only ? ports.m_dffe.m_in_e : ports.m_dffer.m_in_e);
                }
                if (dff_has_reset(type))
                {
                    bool r_only = test_is_reg_with_reset(type);
                    in_ports.emplace(r_only ? ports.m_dffr.m_in_r : ports.m_dffer.m_in_r);
                }
            }
            
            for (const signal_id_t sig_in : in_ports)
            {
                // Raise an error in belong to top module inputs
                if  (belongs_to(sig_in, top_circuit.m_in_ports) &&
                    !belongs_to(sig_in, m_in_ports))
                {
                    std::cout << int(sig_in) << std::endl;
                    throw std::logic_error(ILLEGAL_SUBCIRCUIT_MISSING_INPUT);
                }
                visited_sigs.emplace(sig_in);
            }
            visited_cells.insert(p_cell);
            if (is_register(type)) m_reg_outs.insert(ports.m_dff.m_out_q);
        }
    }

    // Check for useless input
    for (const signal_id_t p_sig : m_in_ports)
    {
        if (visited_sigs.find(p_sig) != visited_sigs.end()) continue;
        std::cout << "Warning subcircuit: useless inputs `"
                  << uint32_t(p_sig) << "`" << std::endl;
        // throw std::logic_error(ILLEGAL_SUBCIRCUIT_USELESS_INPUT);
    }

    // Check for implicit output
    //  - internal wires that belong to top_circuit.m_out_ports
    for (const signal_id_t sig : visited_sigs) {
        if ( sig == signal_id_t::S_0 || sig == signal_id_t::S_1 ||
             sig == signal_id_t::S_X || sig == signal_id_t::S_Z)
        { continue; }
        if (belongs_to(sig, top_circuit.m_out_ports) && !belongs_to(sig, m_out_ports))
        {
            std::cout << "Warning subcircuit: `" << uint32_t(sig) << "`" << std::endl;
            throw std::logic_error(ILLEGAL_SUBCIRCUIT_IMPLICIT_TOPMOD_OUTPUT);
        }
    }

    // Check for implicit outputs
    //  - connected cell not present in COI of visited sigs
    for (const Cell* p_cell : top_circuit.cells())
    {
        if (visited_cells.find(p_cell) != visited_cells.end()) continue;
        const cell_type_t type = p_cell->type();
        const Ports& ports = p_cell->m_ports;
        std::unordered_set<signal_id_t> used_signals;

        #define sigs_visited(x) (visited_sigs.find(x) != visited_sigs.end())

        // Collect unvisited cells inputs 
        if (is_unary(type))
            { used_signals.emplace(ports.m_unr.m_in_a); }
        else if (is_binary(type))
        {
            used_signals.emplace(ports.m_bin.m_in_a);
            used_signals.emplace(ports.m_bin.m_in_b);
        }
        else if (is_multiplexer(type))
        {
            if (sigs_visited(ports.m_mux.m_in_a)) used_signals.emplace(ports.m_mux.m_in_a);
            if (sigs_visited(ports.m_mux.m_in_b)) used_signals.emplace(ports.m_mux.m_in_b);
            if (sigs_visited(ports.m_mux.m_in_s)) used_signals.emplace(ports.m_mux.m_in_s);
        }
        else
        {
            assert(is_register(type));
            used_signals.emplace(ports.m_dff.m_in_d);
            used_signals.emplace(ports.m_dff.m_in_c);

            if (dff_has_enable(type))
            {
                bool e_only = test_is_reg_with_enable(type);
                used_signals.emplace(e_only ? ports.m_dffe.m_in_e : ports.m_dffer.m_in_e);
            }
            if (dff_has_reset(type))
            {
                bool r_only = test_is_reg_with_reset(type);
                used_signals.emplace(r_only ? ports.m_dffr.m_in_r : ports.m_dffer.m_in_r);
            }
        }

        // Test if unvisited cells have inputs connected to the current subcircuit 
        for (const signal_id_t sig : used_signals) {
            if ( sig == signal_id_t::S_0 || sig == signal_id_t::S_1 ||
                 sig == signal_id_t::S_X || sig == signal_id_t::S_Z )
            { continue; }
            if (belongs_to(sig, m_in_ports)) continue;
            if (belongs_to(sig, visited_sigs) && !belongs_to(sig, m_out_ports))
            {
                std::cout << "Warning subcircuit: implicit cell connection `"
                          << uint32_t(sig) << "`, name: " << p_cell->name() << std::endl;
                // throw std::logic_error(ILLEGAL_SUBCIRCUIT_IMPLICIT_CELL_OUTPUT);
            }
        }
    }

    // Copy visited signals (inputs are already present)
    for (const signal_id_t sig : visited_sigs)
        { m_signals.emplace(sig); }

    // Copy visited cells preserving the topological sort, registers first
    m_cells.reserve(visited_cells.size());
    for (const Cell* p_cell : top_circuit.cells())
    {
        if (visited_cells.find(p_cell) != visited_cells.end())
            m_cells.push_back(new Cell(*p_cell));
    }

    // Copy netnames if they are partially included in the subcircuit
    for (const auto& name_bits  : top_circuit.nets())
    {
        const std::string& name = name_bits.first;
        const std::vector<signal_id_t>& bits = name_bits.second;

        // Test if already exists (possibly first within input ports)
        const auto& found_it = m_name_bits.find(name);
        if (found_it != m_name_bits.end())
        {
            const std::vector<signal_id_t>& other = found_it->second;
            if (other.size() != bits.size())
            {
                std::cout << "Error subcircuit: net name redeclaration `"
                          << name << "`" << std::endl;
                throw std::logic_error(ILLEGAL_NAME_REDECLARATION);
            }
            for (size_t i = 0; i < other.size(); i++)
                if (bits[i] != other[i])
                {
                    std::cout << "Error subcircuit : net name redeclaration `"
                              << name << "`" << std::endl;
                    throw std::logic_error(ILLEGAL_NAME_REDECLARATION);
                }
        }
        else
        {
            // Test if it is partially included in the sub circuit
            bool partially_included = false;
            for (const signal_id_t sig : bits) {
                if (m_signals.find(sig) != m_signals.end()) {
                    partially_included = true;
                }
            }

            if (partially_included)
            {
                const auto& emplace_it = m_name_bits.emplace(name, bits);
                add_bit_names(name, bits);
                assert(emplace_it.second);
            }
            // else
            // {
            //     std::cout << "Warning subcircuit: net name`" << name 
            //               << "` is not fully included in the sub circuit" << std::endl;
            // }
        }
    }
}

void Circuit::add_bit_names(const std::string& name, const std::vector<signal_id_t>& typed_signals)
{
    for (uint32_t pos = 0; pos < typed_signals.size(); pos++)
    {
        const signal_id_t sig = typed_signals[pos];
        const VerilogId vid(m_name_bits.find(name)->first, pos);
        auto found = m_bit_name.find(sig);
        if (found == m_bit_name.end())
            m_bit_name.emplace(sig, vid);
        else if (vid < found->second)
            m_bit_name.emplace_hint(found, sig, vid);
    }
}

Circuit::~Circuit()
{
    for (const Cell* p_cell : m_cells)
        { delete p_cell; }

    std::unordered_set<std::unordered_set<signal_id_t>*> to_delete;
    for (const auto& d :d_connected_regs)
        { if (d.second != nullptr) to_delete.emplace(d.second); }
    for (const auto& d :d_connected_outs)
        { if (d.second != nullptr) to_delete.emplace(d.second); }
    for (auto ptr : to_delete) delete ptr;
}

bool Circuit::has(const std::string& name) const
{
    return m_name_bits.find(name) != m_name_bits.end();
}

const std::vector<signal_id_t>& Circuit::operator[](const std::string& name) const
{
    assert(has(name));
    return m_name_bits.at(name);
}

const std::unordered_set<signal_id_t> Circuit::get_prev_regs(const signal_id_t sig) const
{
    assert(m_reg_outs.find(sig) != m_reg_outs.end());
    const auto& f = d_previous_regs.find(sig);
    if (f != d_previous_regs.end()) return f->second;
    const std::unordered_set<signal_id_t> empty;
    return empty;
}

const std::unordered_set<signal_id_t>* Circuit::get_conn_regs(const signal_id_t sig) const
{
    assert(!d_connected_regs.empty());
    return d_connected_regs.at(sig);
}

const std::unordered_set<signal_id_t>* Circuit::get_conn_outs(const signal_id_t sig) const
{
    assert(!d_connected_outs.empty());
    return d_connected_outs.at(sig);
}


std::stringstream Circuit::stats() const
{
    std::stringstream ss;
    ss << "******* Circuit Stats ********" << std::endl;
    ss << "Cells size: " << cells().size() << std::endl;
    ss << "Sigs size: " << sigs().size() << std::endl;
    ss << "Inputs size: " << ins().size() << std::endl;
    ss << "Ouputs size: " << outs().size() << std::endl;
    ss << "Registers size: " << regs().size() << std::endl;
    ss << "Nets size: " << nets().size() << std::endl;
    return ss;
}


void Circuit::build_adjacent_lists()
{
    // Graph traveral to find adjacent vertices
    // First, we build a map between a wire and its adjacent cells
    std::unordered_map<signal_id_t, std::unordered_set<const Cell*>> sig_to_cells;

    for (const Cell* p_cell : m_cells) {
        const Ports& ports = p_cell->ports();

        if (is_unary(p_cell->type())) {
            sig_to_cells[ports.m_unr.m_in_a].emplace(p_cell);        
        } else if (is_binary(p_cell->type())) {
            sig_to_cells[ports.m_bin.m_in_a].emplace(p_cell);
            sig_to_cells[ports.m_bin.m_in_b].emplace(p_cell);
        } else if (is_multiplexer(p_cell->type())) {
            sig_to_cells[ports.m_mux.m_in_a].emplace(p_cell);
            sig_to_cells[ports.m_mux.m_in_b].emplace(p_cell);
            sig_to_cells[ports.m_mux.m_in_s].emplace(p_cell);
        } else {
            assert(is_register(p_cell->type()));
            sig_to_cells[ports.m_dff.m_in_d].emplace(p_cell);
            sig_to_cells[ports.m_dff.m_in_c].emplace(p_cell);
            if (dff_has_enable(p_cell->type()))
            {
                bool e_only = test_is_reg_with_enable(p_cell->type());
                const signal_id_t& s = e_only ? ports.m_dffe.m_in_e : ports.m_dffer.m_in_e;
                sig_to_cells[s].emplace(p_cell);
            }
            if (dff_has_reset(p_cell->type()))
            {
                bool r_only = test_is_reg_with_reset(p_cell->type());
                const signal_id_t& s = r_only ? ports.m_dffr.m_in_r : ports.m_dffer.m_in_r;
                sig_to_cells[s].emplace(p_cell);
            }
        }
    }

    // Build ordered vector of sigs to explore
    std::vector<signal_id_t> sig_order;
    sig_order.reserve(m_in_ports.size() + m_cells.size() + 4);

    sig_order.push_back(signal_id_t::S_0);
    sig_order.push_back(signal_id_t::S_1);
    sig_order.push_back(signal_id_t::S_X);
    sig_order.push_back(signal_id_t::S_Z);

    for (const auto& sig : m_in_ports) {
        if ( sig != signal_id_t::S_0 && sig != signal_id_t::S_1 &&
             sig != signal_id_t::S_X && sig != signal_id_t::S_Z)
        sig_order.push_back(sig);
    }

    for (const auto& cell : m_cells)
    {
        const Ports& c_ports = cell->ports();
        assert(&(c_ports.m_unr.m_out_y) == &(c_ports.m_bin.m_out_y));
        assert(&(c_ports.m_unr.m_out_y) == &(c_ports.m_mux.m_out_y));
        assert(&(c_ports.m_unr.m_out_y) == &(c_ports.m_dff.m_out_q));
        sig_order.push_back(c_ports.m_unr.m_out_y);
    }

    std::unordered_set<signal_id_t>* empty = new std::unordered_set<signal_id_t>;

    // Backward traversal to computed reg neighbours and outputs neighbours
    for (auto it = sig_order.rbegin(); it != sig_order.rend(); it++)
    {
        assert(d_connected_outs[*it] == nullptr);
        assert(d_connected_regs[*it] == nullptr);

        // Set of pointers to register list
        std::unordered_set<std::unordered_set<signal_id_t>*> reg_list__set;
        std::unordered_set<signal_id_t>* regs = new std::unordered_set<signal_id_t>;

        // Set of pointers to output list
        std::unordered_set<std::unordered_set<signal_id_t>*> output_list__set;
        std::unordered_set<signal_id_t>* outputs = new std::unordered_set<signal_id_t>;

        // If signal is itself an output, add it to set
        if(m_out_ports.find(*it) != m_out_ports.end()) outputs->emplace(*it);

        // iterate over cells connected to signal it
        for (const Cell* c : sig_to_cells[*it]) {

            if (is_register(c->type())) {
                regs->emplace(c->ports().m_dff.m_out_q);
            } else {
                const Ports& ports = c->ports();
                assert(&(ports.m_unr.m_out_y) == &(ports.m_bin.m_out_y));
                assert(&(ports.m_unr.m_out_y) == &(ports.m_mux.m_out_y));
                assert(&(ports.m_unr.m_out_y) == &(ports.m_dff.m_out_q));
                const signal_id_t out_y = ports.m_unr.m_out_y;

                // get register list pointer and emplace it
                if (!d_connected_regs[out_y]->empty()) {
                    reg_list__set.emplace(d_connected_regs[out_y]);
                }

                // get output list pointer and emplace it
                if (!d_connected_outs[out_y]->empty()) {
                    output_list__set.emplace(d_connected_outs[out_y]);
                }
            }
        }

        if (reg_list__set.empty() && regs->empty()) {
            d_connected_regs[*it] = empty;
            delete regs;
        } else if (reg_list__set.empty() && !regs->empty()) {
           d_connected_regs[*it] = regs;
        } else if((reg_list__set.size() == 1) && regs->empty()) {
            d_connected_regs[*it] = *next(reg_list__set.begin(),0);
            delete regs;
        } else if(!reg_list__set.empty()) {
            for (std::unordered_set<signal_id_t>* reg_list : reg_list__set) {
                regs->insert(reg_list->begin(), reg_list->end());
                d_connected_regs[*it] = regs;
            }
        } else { assert(false); }

        if (output_list__set.empty() && outputs->empty()) {
            d_connected_outs[*it] = empty;
            delete outputs;
        } else if (output_list__set.empty() && !outputs->empty()) {
           d_connected_outs[*it] = outputs;
        } else if((output_list__set.size() == 1) && outputs->empty()) {
            d_connected_outs[*it] = *next(output_list__set.begin(),0);
            delete outputs;
        } else if(!output_list__set.empty()) {
            for (std::unordered_set<signal_id_t>* output_list : output_list__set) {
                outputs->insert(output_list->begin(), output_list->end());
                d_connected_outs[*it] = outputs;
            }
        } else { assert(false); }
    }

    // Computed previous regs for each register
    for (const auto& sig : m_reg_outs) {
        for (const auto& conn_reg : *d_connected_regs[sig]) {
            d_previous_regs[conn_reg].emplace(sig);
        }
    }
}
