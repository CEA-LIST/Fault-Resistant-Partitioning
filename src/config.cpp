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

#include "config.h"

constexpr const char* MISSING_PARAM = "Missing parameter in configuration file";
constexpr const char* MISSING_CONF = "Missing configuration in file";


config_t::config_t( std::string config_file,
                    std::string config_name)
{
    std::ifstream f; f.exceptions(std::ifstream::badbit);
    f.open(config_file);
    std::string data(std::istreambuf_iterator<char>{f}, {});
    f.close();
    auto pdata = nlohmann::json::parse(data);

    if (!pdata.contains(config_name))
        throw std::logic_error(MISSING_CONF);

    auto jdata = pdata.at(config_name);

    try {
        design_path = jdata.at("design_path");
        design_name = jdata.at("design_name");
        k = jdata.at("k");
        delay = jdata.at("delay");
        dump_path = jdata.at("dump_path");

        for (const auto& alert: jdata.at("alert_list").items())
        {
            std::string alert_name = alert.key();
            const auto& alert_value = alert.value();
            if (!alert_value.is_array())
                { throw std::logic_error(ILLEGAL_SIGNAL_LIST);}

            // Register alert name with empty bool values
            const auto emplace_it = alert_list.emplace(alert_name, std::vector<bool>());
            // // If we fail, it is due to a re-definition of the same name
            // if (!emplace_it.second)
            //     { throw std::logic_error(ILLEGAL_NAME_REDECLARATION); }
            std::vector<bool>& alert_bits = emplace_it.first->second;
            alert_bits.reserve(alert_value.size());
            for (const uint32_t bit : alert_value) {
                alert_bits.push_back(static_cast<bool>(bit));
            }
        }        
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        throw std::logic_error(MISSING_PARAM);
    }


    if (jdata.contains("invariant_list"))
    for (const auto& invariant: jdata.at("invariant_list").items())
    {
        std::string invariant_name = invariant.key();
        const auto& invariant_value = invariant.value();
        if (!invariant_value.is_array())
            { throw std::logic_error(ILLEGAL_SIGNAL_LIST);}

        // Register alert name with empty bool values
        const auto emplace_it = invariant_list.emplace(invariant_name, std::vector<bool>());
        // // If we fail, it is due to a re-definition of the same name
        // if (!emplace_it.second)
        //     { throw std::logic_error(ILLEGAL_NAME_REDECLARATION); }
        std::vector<bool>& invariant_bits = emplace_it.first->second;
        invariant_bits.reserve(invariant_value.size());
        for (const uint32_t bit : invariant_value) {
            invariant_bits.push_back(static_cast<bool>(bit));
        }
    }

    if (jdata.contains("subcircuit"))
        { subcircuit = jdata.at("subcircuit"); }
    else subcircuit = false;

    if (subcircuit) {
        subcircuit_interface_path = jdata.at("subcircuit_interface_path");
        subcircuit_interface_name = jdata.at("subcircuit_interface_name");
    }

    if (jdata.contains("initial_partition_path"))
        { initial_partition_path = jdata.at("initial_partition_path"); }

    if (jdata.contains("f_included_prefix"))
        { f_included_prefix = jdata.at("f_included_prefix"); }
    
    if (jdata.contains("f_excluded_prefix"))
        { f_excluded_prefix = jdata.at("f_excluded_prefix"); }
    
    if (jdata.contains("f_gates"))
        { f_gates = static_cast<gates_t>(jdata.at("f_gates")); }
    else f_gates = ALL ;
    
    if (jdata.contains("exclude_inputs"))
        { exclude_inputs = jdata.at("exclude_inputs"); }
    else exclude_inputs = false ;

    if (jdata.contains("f_effect"))
        { f_effect = jdata.at("f_effect"); }

    // Dump infos
    if (jdata.contains("enumerate_exploitable"))
        { enumerate_exploitable = jdata.at("enumerate_exploitable"); }
    else enumerate_exploitable = false ;

    if (jdata.contains("optim_atleast2"))
        { optim_atleast2 = jdata.at("optim_atleast2"); }
    else optim_atleast2 = true ;

    if (jdata.contains("dump_vcd"))
        { dump_vcd = jdata.at("dump_vcd"); }
    else dump_vcd = false ;

    if (jdata.contains("dump_partitioning"))
        { dump_partitioning = jdata.at("dump_partitioning"); }
    else dump_partitioning = true ;

    if (jdata.contains("increasing_k"))
        { increasing_k = jdata.at("increasing_k"); }
    else increasing_k = true ;

    if (jdata.contains("interesting_names"))
        { interesting_names = jdata.at("interesting_names"); }

    if (jdata.contains("procedure"))
        { procedure = static_cast<procedure_t>(jdata.at("procedure")); }
    else procedure = BOTH ;

    if (jdata.contains("f_excluded_signals")) {
        for (const signal_id_t sig : jdata.at("f_excluded_signals")) {
            f_excluded_signals.push_back(sig);
        }
    }


    if (std::filesystem::exists(dump_path)) {
        std::filesystem::remove_all(dump_path);
        // throw std::runtime_error("Output folder `" + dump_path + "` already exists");
    }

    std::filesystem::create_directories(dump_path);
    std::filesystem::copy_file(config_file, dump_path+"/config_file");
}
