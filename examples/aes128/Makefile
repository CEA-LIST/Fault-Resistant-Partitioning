YOSYS       := yosys
YOSYS_ARGS  := -m ghdl

CURRENT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
RTL_PATH := ${CURRENT_DIR}../ImpeccableCircuits/AES/AES128_red1,4,5,8/RTL
SRC_LIST := src.list
PREP_YS  := prep.ys
LOG_YS   := log
WORK_YS := work_yosys


copy_src:
ifeq (,$(wildcard $(RTL_PATH)))
	$(info RTL src not found: $(RTL_PATH))
	$(info Make sur you fetch 'ImpeccableCircuits' submodules)
else 
	cp -r $(RTL_PATH) $(CURRENT_DIR)
endif

yosys: copy_src
	$(info Process VHDL files with ghdl)
	mkdir -p $(WORK_YS)
	while read -r line; \
		do ghdl -a --workdir=$(WORK_YS) $$line ; \
	done < $(SRC_LIST)
	cd $(WORK_YS); \
	$(YOSYS) $(YOSYS_ARGS) ../$(PREP_YS) > $(LOG_YS)

clean:
	rm -rf $(WORK_YS) RTL/
