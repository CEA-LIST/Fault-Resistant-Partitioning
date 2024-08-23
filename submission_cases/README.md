# Submission cases

This folder provides the case studies presented and analyzed in our [Paper](#publication) for reproducibility.

This folder provides circuits at the gate-level netlist in `json` and `verilog` formats.
Optionally, an `*_interface.json` file specifies the input/output interface of subcircuits to analyze them separately.
This folder also provides the partitioning results in `*_partitions.json` for Secure Ibex.
These files should be obtained using the *k-partitions* program but can also be used to initialize the algorithm with a pre-computed partitioning.
See the `example` folder to know how these netlists are built from the RTL description using Yosys.

Here are the available use cases (see `config_file.json` file):

- **Skinny64** from [Impeccable Circuits](https://github.com/emsec/ImpeccableCircuits/tree/master)
  - skinny_red1
  - skinny_red3
  - skinny_red4

- **AES128** from [Impeccable Circuits](https://github.com/emsec/ImpeccableCircuits/tree/master)
  - aes_red1
  - aes_red4
  - aes_red5

- **Secure Ibex** from [GitHub](https://github.com/lowRISC/ibex) as used in OpenTitan
  - ibex: baseline secure version (commit ce536ae4)
  - ibex_fixed_rf: updated version with the register file security patch (commit 35bbdb7b)
  - ibex_d3: same as *ibex_fixed_rf* with a lockstep delay $d=3$ instead of $d=2$.

The following sections describe how to reproduce results step by step for **skinny_red3** and the **ibex registerfile**.
In particular, we detail how to match experimental results with those presented in the Paper.
Every other use case specified in `config_file.json` is, of course, functional but not detailed here.

> Be careful before running the proofs! Verification times can be extremely long. See Tables 1 and 2 in our [Paper](#publication) for details.


## skinny_red3

Running the following command from the root directory produces the `out/skinny_red3/log` log file.
```bash
$ ./build/k-partitions skinny_red3
```

Inspecting the log file reveals the circuit has 2959 cells (line 2), including 305 registers (line 6).

Procedure 1 terminates in 9.06s (line 92) with 305 partitions (line 90).

In contrast, Procedure 2 identifies and enumerates multiple exploitable partitions (line 653) until the solver returns UNSAT (line 655).
Exploitable faults were found to correspond to the 64 unprotected circuit outputs.
Procedure 2 terminates in 0.324s (line 656).

An example of what you should observe in `out/skinny_red3/log` is reproduced below:
```
1     ******* Circuit Stats ********
2     Cells size: 2959
3     Sigs size: 3093
4     Inputs size: 130
5     Ouputs size: 65
6     Registers size: 305
7     Nets size: 6299

...

89    Running solver 7: 0.609 s ->  UNSAT
90    Partitioning finished with 305 partitions.
91    Write partitioning in file `out/skinny_red3/partitioning-7.json`
92    => Procedure 1 verification time: 9.06 s

...

653   Enumerate exploitable partitions: 212 192 191 190 [...]
654
655   Running solver 71: UNSAT 0.0 s
656   => Procedure 2 verification time: 0.324 s
```

Results in Table 1 (cf. [Paper](#publication)) must correspond to the observed experimental value.


## ibex register_file

Secure Ibex implements multiple hardware fault protections like dual-core lockstep and register-file error code detection.
The following describes how to reproduce results for the register file.

Running the following command from the root directory produces the `out/regfile_k1/log` log file.
```bash
$ ./build/k-partitions regfile_k1
```

Inspecting the log file reveals the circuit has 8332 cells (line 2), including 1326 registers (line 6).

Procedure 1 enumerates 172 exploitable faults (line 3136) and terminates in 38s (line 3141) with 1326 partitions (line 3139).

Similarly, Procedure 2 identifies those 172 exploitable partitions (line 653) until the solver returns UNSAT (line 655).
Procedure 2 terminates in 53s (line 656).

An example of what you should observe in `out/regfile_k1/log` is reproduced below:
```
1     ******* Circuit Stats ********
2     Cells size: 8332
3     Sigs size: 8396
4     Inputs size: 60
5     Ouputs size: 137
6     Registers size: 1326
7     Nets size: 10238

...

3136  Enumerate exploitable faults: 67219 67226 67225 67218 [...]
3137
3138  Running solver 174: 6.037 s ->  UNSAT
3139  Partitioning finished with 1326 partitions.
3140  Write partitioning in file `out/regfile_k1/partitioning-174.json`
3141  => Procedure 1 verification time: 38 s

...

4882  Enumerate exploitable faults: 67226 67203 67194 67208 [...]
4883  Enumerate exploitable partitions: 
4884  
4885    Running solver 346: UNSAT 32.346 s
4886  => Procedure 2 verification time: 53 s
```

Results in Table 2 in the [Paper](#publication) must correspond to the observed experimental value.

Running` $ ./build/k-partitions regfile_fixed` shows that the 172 exploitable fault locations have been fixed.