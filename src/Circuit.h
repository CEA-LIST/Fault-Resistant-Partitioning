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

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <string>
#include <cstdint>
#include <utility>
#include <cassert>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "Cell.h"
#include "VerilogId.h"

constexpr const char* ILLEGAL_SUBCIRCUIT_MISSING_OUTPUT  = "Illegal top_module output is reachable when extracting subcircuit";
constexpr const char* ILLEGAL_SUBCIRCUIT_IMPLICIT_CELL_OUTPUT     = "Implicit subcircuit output: external cell connected to subcircuit internal signal";
constexpr const char* ILLEGAL_SUBCIRCUIT_IMPLICIT_TOPMOD_OUTPUT   = "Implicit subcircuit output: subcircuit internal signal used by topcircuit as an output";
constexpr const char* ILLEGAL_SUBCIRCUIT_MISSING_INPUT   = "Missing inputs when extracting subcircuit";
constexpr const char* ILLEGAL_SUBCIRCUIT_USELESS_INPUT   = "Unconnected input when extracting subcircuit";

class Circuit
{
private:
    void add_bit_names(const std::string& name, const std::vector<signal_id_t>& typed_signals);
protected:
    std::unordered_set<signal_id_t> m_in_ports;
    std::unordered_set<signal_id_t> m_out_ports;
    std::unordered_set<signal_id_t> m_reg_outs;
    std::unordered_set<signal_id_t> m_signals;
    std::vector<const Cell*> m_cells;
    std::unordered_map<std::string, std::vector<signal_id_t>> m_name_bits;
    std::unordered_map<signal_id_t, std::unordered_set<signal_id_t>*> d_connected_regs;
    std::unordered_map<signal_id_t, std::unordered_set<signal_id_t>*> d_connected_outs;
    std::unordered_map<signal_id_t, std::unordered_set<signal_id_t>> d_previous_regs;
    std::unordered_map<signal_id_t, VerilogId> m_bit_name;
    std::string m_module_name;
    signal_id_t m_sig_clock;
    template <typename T> signal_id_t get_signal_any(T& bit);
    Circuit(const Circuit& circ) = default;
public:
    Circuit(const std::string& json_file_path, const std::string& top_module_name);
    Circuit(const Circuit& top_circuit, const std::string& subcircuit_file, const std::string& top_module_name);
    bool has(const std::string& name) const;
    const std::vector<const Cell*>& cells() const { return m_cells; };
    const std::unordered_set<signal_id_t>& ins() const { return m_in_ports; };
    const std::unordered_set<signal_id_t>& sigs() const { return m_signals; };
    const std::unordered_set<signal_id_t>& outs() const { return m_out_ports; };
    const std::unordered_set<signal_id_t>& regs() const { return m_reg_outs; };
    const std::unordered_map<std::string, std::vector<signal_id_t>>& nets() const { return m_name_bits; };
    signal_id_t clock() const { return m_sig_clock; };
    const std::unordered_set<signal_id_t>* get_conn_regs(const signal_id_t sig) const;
    const std::unordered_set<signal_id_t>* get_conn_outs(const signal_id_t sig) const;
    const std::unordered_set<signal_id_t> get_prev_regs(const signal_id_t sig) const;
    VerilogId bit_name(const signal_id_t sig) const { return m_bit_name.at(sig); }
    const std::vector<signal_id_t>& operator[](const std::string& name) const;
    std::stringstream stats() const;
    void build_adjacent_lists();
    ~Circuit();
};

template <typename T> signal_id_t Circuit::get_signal_any(T& bit)
{
    if(bit.is_number_unsigned())
        return static_cast<signal_id_t>((uint32_t)bit);
    else if(bit.is_string()) // it must be a constant
        return signal_from_str(bit);
    throw std::logic_error(ILLEGAL_SIGNAL_TYPE);
}

#endif // CIRCUIT_H
