build: clean
	@gcc main.c -o intel-cpu-tuner
	@chmod +x intel-cpu-tuner

install: build
	@cp intel-cpu-tuner /usr/bin/intel-cpu-tuner
	@chown 0:0 /usr/bin/intel-cpu-tuner
	@chmod 0755 /usr/bin/intel-cpu-tuner

uninstall:
	@rm -f /usr/bin/intel-cpu-tuner

clean:
	@rm -f intel-cpu-tuner
