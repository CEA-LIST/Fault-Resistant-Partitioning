YOSYS       := yosys

CURRENT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

IBEX_PATH := ${CURRENT_DIR}../ibex
SRC_LIST := src.list
PREP_YS  := prep.ys
LOG_YS   := log
WORK_YS := work_yosys
WORK_DIR := RTL


copy_src:
ifeq (,$(wildcard $(IBEX_PATH)/rtl))
	$(info RTL src not found: $(IBEX_PATH)/rtl)
	$(info Make sur you fetch 'ibex' submodule)
else
	$(info Build Ibex core from repository...)
	$(shell ./script.sh copy_src $(IBEX_PATH) $(WORK_DIR))
endif

convert: copy_src
	$(info Convert SystemVerilog files to Verilog using sv2v)
	$(shell ./script.sh sv2v_convert $(WORK_DIR))

yosys: convert
	$(info Synthesize Netlist using Yosys)
	mkdir -p $(WORK_YS)
	cd $(WORK_YS); \
	$(YOSYS) ../$(PREP_YS) > $(LOG_YS)

clean:
	rm -rf build $(WORK_YS) $(WORK_DIR)

