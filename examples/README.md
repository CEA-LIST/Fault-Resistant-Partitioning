# Examples

This folder provides examples of how to synthesize circuit netlists with Yosys before evaluating them with our *k-partitioning* methodology.
These examples are not meant to be "push-button" but provide enough information for a user who wants to adapt our work to his own use case.

## Overview

Three designs are synthesized here:
- Ibex ([github link](https://github.com/lowRISC/ibex))
- AES 128 ([github link](https://github.com/emsec/ImpeccableCircuits/tree/master/AES))
- Skinny 64 ([github link](https://github.com/emsec/ImpeccableCircuits/tree/master/Skinny64))

## Dependencies

You need to install the following dependencies on your own.
- Yosys: https://github.com/YosysHQ/yosys
- sv2v: https://github.com/zachjs/sv2v
- ghdl: https://github.com/ghdl/ghdl
- fusesoc: https://github.com/olofk/fusesoc

Make sure you cloned this repository with its `ImpeccableCircuits` and `ibex` submodules:

```
git submodule update --init --recursive ibex ImpeccableCircuits
```
## Usage

Run `make yosys` in the desired folder:
- aes128
- secure_ibex
- skinny64

This command should copy the source RTL into an `RTL` folder.
Yosys then generates a circuit netlist in .json format in the `work_yosys` folder.
This netlist can be provided to the *k-partitions* algorithm as illustrated in the `../submission_cases` folder.
