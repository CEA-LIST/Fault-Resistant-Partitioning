# Config File format

This `config_file.json` file specifies the configuration used to execute the fault partitioning algorithm.
A configuration can be provided to the `k-partitions` program when executing the analysis.
Otherwise, the *default* configuration parameters are used.

Each configuration has the following attributes:

## Design Specification
| name                      | type                      | required | default | description                                                                                   |
| :------------------------ | :------------------------ | :------: | :-----: | :-------------------------------------------------------------------------------------------- |
| design_path               | string                    |   yes    |         | Path to the netlist in json format                                                            |
| design_name               | string                    |   yes    |         | Design name in the provided json netlist                                                      |
| subcircuit                | bool                      |    no    |  false  | A subcircuit interface must be provided when set to true                                      |
| subcircuit_interface_path | string                    |    no    |   ""    | Path to the subcircuit interface in json format                                               |
| subcircuit_interface_name | string                    |    no    |   ""    | Name of the subcircuit interface to read in the provided json file                            |
| alert_list                | map<string, vector<bool>> |   yes    |         | List of <alert name, value> pairs specifying alert values when not triggered                  |
| invariant_list            | map<string, vector<bool>> |    no    |   {}    | List of <signal name, value> pairs specifying invariants in the golden trace at initial state |
| initial_partition_path    | string                    |    no    |   ""    | Path to the initial circuit partitioning in json format                                       |
| delay                     | uint                      |   yes    |         | Alert delay of the concurrent error detection scheme                                          |

## Fault Model

| name               | type            | required | default | description                                                                                                                     |
| :----------------- | :-------------- | :------: | :-----: | :------------------------------------------------------------------------------------------------------------------------------ |
| f_included_prefix  | vector\<string> |    no    |   {}    | When not empty, faults are only injected in gates starting with the provided prefixes. Faults are injected everywhere otherwise |
| f_gates            | {0, 1}          |    no    |    0    | Inject faults in each gate (0) or only in sequential gates (1)                                                                  |
| f_excluded_prefix  | vector\<string> |    no    |   {}    | Faults are never injected to gates starting with the provided prefixes                                                          |
| f_excluded_signals | vector\<uint>   |    no    |   {}    | Faults are never injected to gates matching the provided IDs                                                                    |
| exclude_inputs     | bool            |    no    |  false  | Inputs cannot be faulted when set to true                                                                                       |
| k                  | uint            |   yes    |         | Maximal number of fault injections                                                                                              |
| increasing_k       | bool            |    no    |  true   | Start the analysis with small values of k                                                                                       |
| procedure          | {0,1,2}         |    no    |    0    | Only apply Procedure 1 or 2. Apply both when set to 0                                                                           |

## Optimization

| name                  | type | required | default | description                                                                            |
| :-------------------- | :--- | :------: | :-----: | :------------------------------------------------------------------------------------- |
| optim_atleast2        | bool |    no    |  true   | Do not fault cells connected to at most 1 register. Applies to procedure 1 only.       |
| enumerate_exploitable | bool |    no    |  false  | Enumerate exploitable fault locations instead of merging partitions during Procedure 1 |

## Dump

| name              | type            | required | default | description                                                                              |
| :---------------- | :-------------- | :------: | :-----: | :--------------------------------------------------------------------------------------- |
| dump_path         | string          |   yes    |         | Path to the analysis output directory                                                    |
| dump_vcd          | bool            |    no    |  false  | Dump VCD traces for each iteration of the algorithm                                      |
| dump_partitioning | bool            |    no    |  true   | Dump circuit partitioning for each fixed point reached during Procedure 1                |
| interesting_names | vector\<string> |    no    |   {}    | Print if the built partitions contain gates starting with the provided interesting names |
