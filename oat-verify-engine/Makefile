TEST ?= a.out.strip
objdump ?= aarch64-linux-gnu-objdump 
readelf ?= aarch64-linux-gnu-readelf
hikey ?= linaro@192.168.1.103
CONFIG_SCRIPT ?= gen_config.py

backup:
	cp $(TEST) $(TEST).bak

replay: config
	./verify_engine -c replay.cfg -t tracefile.txt -o debugtrace.txt -v -v -v -l $(TEST)

dump:
	$(objdump) $(TEST) -D > $(TEST).dump

ph: $(TEST)
	$(readelf) -S $(TEST) > $(TEST).ph 
	cat $(TEST).ph

sh: $(TEST)
	$(readelf) -l $(TEST) > $(TEST).sh 
	cat $(TEST).sh

config:	ph dump
	./$(CONFIG_SCRIPT) $(TEST).dump --program-header $(TEST).ph -o replay.cfg
