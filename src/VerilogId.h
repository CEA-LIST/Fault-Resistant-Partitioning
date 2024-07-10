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

#ifndef VERILOGID_H
#define VERILOGID_H

#include <inttypes.h>
#include <string>

class VerilogId {
    const std::string* m_name;
    uint32_t m_pos;
    uint32_t m_depth;
public:
    constexpr VerilogId(const std::string& name, uint32_t pos);
    const std::string& name() const { return *m_name; }
    uint32_t pos() const { return m_pos; }
    uint32_t depth() const { return m_depth; }
    std::string display() const { return *m_name + " [" + std::to_string(m_pos) + "]"; }
};

constexpr VerilogId::VerilogId(const std::string& name, uint32_t pos)
    : m_name(&name), m_pos(pos), m_depth(0)
{
    size_t curr = 0;
    size_t found = 0;
    do {
        found = m_name->find('.', curr);
        curr = found + 1;
        m_depth += 1;
    } while(found != std::string::npos);
}

inline bool operator==(const VerilogId& a, const VerilogId& b)
{ return a.pos() == b.pos() && a.depth() == b.depth() && a.name() == b.name(); }

inline bool operator<(const VerilogId& a, const VerilogId& b)
{
    const size_t apos = a.name().find('_'),
                 bpos = b.name().find('_');
    if (bpos == 0 && apos != 0)
        return true;
    else if (bpos != 0 && apos == 0)
        return false;

    if (a.depth() < b.depth())
        return true;
    else if (a.depth() > b.depth())
        return false;

    return (a.name().length() < b.name().length());
}

#endif // VERILOGID_H
