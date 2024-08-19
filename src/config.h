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

#ifndef VERIFIER_CONFIG_H
#define VERIFIER_CONFIG_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "json.hpp"
#include "Circuit.h"
#include "vars.h"
#include "Solver.h"

typedef enum {BOTH, PROC_1, PROC_2} procedure_t;
typedef enum {ALL, SEQ} gates_t;

struct config_t
{
    procedure_t procedure;

    // Design info
    std::string design_path;
    std::string design_name;
    std::uint32_t delay;


    bool subcircuit;
    std::string subcircuit_interface_path;
    std::string subcircuit_interface_name;
    std::unordered_map<std::string, std::vector<bool>> alert_list;
    std::unordered_map<std::string, std::vector<bool>> invariant_list;
    std::string initial_partition_path;

    // Fault infos
    std::vector<std::string> f_included_prefix;
    std::vector<std::string> f_excluded_prefix;
    std::vector<signal_id_t> f_excluded_signals;
    gates_t f_gates;
    bool exclude_inputs;
    uint32_t k;
    bool increasing_k;
    std::string f_effect;

    // Dump infos
    std::string dump_path;
    bool enumerate_exploitable;
    bool optim_atleast2;
    bool dump_vcd;
    bool dump_partitioning;
    std::vector<std::string> interesting_names;
    
    config_t(std::string config_file, std::string config_name);
};


#endif // VERIFIER_CONFIG_H
