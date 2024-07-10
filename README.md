# Fault-Resistant Partitioning

This repository contains the source code for the paper  *Fault-Resistant Partitioning of Secure CPUs for System Co-Verification against Faults* (see [Publication](#publication)).


## TL;DR

```sh
# Build tool using Cmake
cmake src -Bbuild && cmake --build build

# The following executes two use cases which run in a reasonable amount of time

# Evaluate Skinny64's robustness to 2 faults (10s) and print verification logs
./build/k-partitions skinny_red3 &&
less out/skinny_red3/log

# Evaluate Ibex's Register file robustness to 1 faults (1min 30) and print verification logs
./build/k-partitions regfile_k1 &&
less out/regfile_k1/log
```

## Overview
The repository is structured as follows:
- `src`: Algorithm source code for evaluating circuit robustness against fault injections.
- `submission_cases`: Case studies evaluated in the paper.
- `example`: Examples illustrating how to adapt the tool to your own use cases.
- `config`: Configuration files for the tool.


## Install and Build

Clone this repository along with its `cxxsat` and `json` submodules:
```
git clone <this_repo_URL> --recurse-submodules
```
or initialize and update the submodules if you've already cloned the repository:
```
git submodule update --init --recursive src/cxxsat src/json
```
Build the executable using Cmake:
```
cmake src -Bbuild
cmake --build build
```

The project is built in the `build` directory.

> Alternatively, a Dockerfile sets the execution environment in a container and builds the `k-partitions` tool in the current folder using a mount point
> ```
> docker build -t kfrp-artifact .
> docker run -v $(pwd):/artifact -it kfrp-artifact


## Usage

Define the use cases to be evaluated in the configuration file `config/config_file.json`. See the [README.md](./config/README.md) in the `config` folder for details.

Run the executable with your chosen configuration:
```
./build/k-partitions [CONFIG_name]
```

Refer to the `submission_cases` folder to reproduce examples from our paper.


## Results Interpretations

Unless specified otherwise in `conf_file.json`, the partitioning analysis produces a `log` file in the `out` folder.
The following sections explain the output format.

### Design info

Initially, the design characteristics and initial partitioning information are displayed:
```
******* Circuit Stats ********
Cells size: 8488
Sigs size: 8552
Inputs size: 60
Ouputs size: 137
Registers size: 1404
Nets size: 10311

******* Partition info ********
Number of partitions: 1404
Largest partitions: (0: 1) (1: 1) (2: 1) (3: 1) (4: 1) (5: 1)
Contents of 0: (gen_lockstep: 1) (gen_regfile_ff: 0)
Contents of 1: (gen_lockstep: 1) (gen_regfile_ff: 0)
Contents of 2: (gen_lockstep: 1) (gen_regfile_ff: 0)
Contents of 3: (gen_lockstep: 1) (gen_regfile_ff: 0)
```

The *Partition info* shows the size of the largest partitions in the <idx: size> format.
The content of the four largest partitions is given according to design modules.
The *Partition info* is displayed for each algorithm iteration to track the evolution of partition construction.


### Procedure 1 -- Build partitions

Procedure 1 corresponds to *BuildPartitioning* in Algorithm 1 of our [Publication](#publication).
For each iteration, a budget of *k* faults is divided between *faulty partitions* and *combinational gates*.
A SAT query is sent to a solver, and partitions are merged if SAT.
The algorithm proceeds to the next iteration otherwise.
An example of log output from one iteration is given below:

```
-------------------------------------------------------------
Partitioning for 0/341 faulty partitions,
1/3411 combinational faults at initial state,
and 0/6822 combinational faults in the following clock cycles.
-------------------------------------------------------------

Running solver 1: 0.11 s ->  SAT 
  - Faulty comb gates at clock cycle 0: 629 (register_file_i._66_) 
  - Faulty comb gates at clock cycle 1: 
  - Faulty comb gates at clock cycle 2: 
  - Faulty partitions (initial): 
  - Faulty partitions (next): 2 ( 328 ) 4 ( 322 ) 8 ( 320 ) 9 ( 352 )

  Merge together : 2 4
  Merge together : 8 9
  Merged: 4, Remaining: 339
```

Upon completion, Procedure 1 displays a summary:

```
Partitioning finished with 1404 partitions.
Write partitioning in file `out/regfile_d3_k1/partitioning-174.json`
=> Procedure 1 verification time: 33.348 s
```

### Procedure 2 -- Check output integrity

Procedure 2 corresponds to *CheckIntegrity* in Algorithm 1 of our [Publication](#publication).
Similar to Procedure 1, a budget of *k* faults is divided between faulty partitions and combinational gates for each iteration.
When the solver answers SAT, an exploitable fault location has been identified.

Finally, Procedure 2 enumerates all exploitable fault locations and partitions:
```
Enumerate exploitable faults: 372 379 404 416 434 873 856
Enumerate exploitable partitions: 
Running solver: UNSAT 3.41 s
=> Procedure 2 verification time: 5.67 s
```


## Publication
Tollec, S., Hadžić, V., Nasahl, P., Asavoae, M., Bloem, R., Couroussé, D., Heydemann, K., Jan, M., & Mangard, S. (2024). [Fault-Resistant Partitioning of Secure CPUs for System Co-Verification against Faults](https://eprint.iacr.org/2024/247.pdf).
*IACR Transactions on Cryptographic Hardware and Embedded Systems (CHES)*, 2024.


## Licensing
Unless otherwise noted, everything in this repository is covered by the Apache License, Version 2.0 (see [LICENSE](./LICENSE) for full text).
