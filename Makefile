BINARY = frostd

CC     = gcc
CFLAGS = -std=c2x -D_GNU_SOURCE -Wall -Wextra -Wpedantic \
         $(shell pkg-config --cflags libcyaml libipmimonitoring libfreeipmi libmicrohttpd)
LIBS   = $(shell pkg-config --libs libcyaml libipmimonitoring libfreeipmi libmicrohttpd) \
         -lpthread -lm

TEST_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags cmocka)
TEST_LIBS   = $(LIBS) $(shell pkg-config --libs cmocka)

SRCS = src/config.c src/cpu.c src/fan.c src/gpu.c src/ipmi.c \
       src/log.c src/metrics.c src/prometheus.c src/service.c

TEST_SRCS = tests/test_config.c tests/test_metrics.c tests/test_fan.c \
            tests/test_gpu.c tests/test_cpu.c

.PHONY: build
build:
	$(CC) $(CFLAGS) -o $(BINARY) src/main.c $(SRCS) $(LIBS)

.PHONY: run
run: build
	./$(BINARY) -c dev.yaml

.PHONY: test
test:
	$(CC) $(TEST_CFLAGS) -o /tmp/test_config  src/config.c src/log.c \
	    tests/test_config.c $(TEST_LIBS) && /tmp/test_config
	$(CC) $(TEST_CFLAGS) -o /tmp/test_metrics src/metrics.c \
	    tests/test_metrics.c tests/test_helpers.c $(TEST_LIBS) && /tmp/test_metrics
	$(CC) $(TEST_CFLAGS) -o /tmp/test_fan     src/fan.c src/metrics.c \
	    tests/test_fan.c tests/test_helpers.c $(TEST_LIBS) -lm && /tmp/test_fan
	$(CC) $(TEST_CFLAGS) -o /tmp/test_gpu     src/gpu.c \
	    tests/test_gpu.c tests/test_helpers.c $(TEST_LIBS) && /tmp/test_gpu
	$(CC) $(TEST_CFLAGS) -o /tmp/test_cpu     src/cpu.c \
	    tests/test_cpu.c tests/test_helpers.c $(TEST_LIBS) && /tmp/test_cpu

.PHONY: lint
lint:
	cppcheck --std=c2x --enable=warning,style,performance \
	    --suppress=missingIncludeSystem \
	    --suppress=syntaxError \
	    src/ 2>&1

.PHONY: clean
clean:
	rm -f $(BINARY)

.PHONY: install
install: build
	test -f /etc/frostd.yaml || sudo cp frostd.yaml /etc/frostd.yaml
	sudo cp packaging/frostd.logrotate /etc/logrotate.d/frostd
	sudo sed "s|ExecStart=.*|ExecStart=$(CURDIR)/$(BINARY)|" \
	    packaging/frostd.service \
	    | sudo tee /etc/systemd/system/frostd.service > /dev/null
	sudo systemctl daemon-reload
	sudo systemctl enable frostd
	sudo systemctl start frostd

.PHONY: uninstall
uninstall:
	sudo systemctl stop frostd || true
	sudo systemctl disable frostd || true
	sudo rm -f /etc/systemd/system/frostd.service
	sudo systemctl daemon-reload

.PHONY: uninstall_clean
uninstall_clean: uninstall
	LOG_FILE=$$(awk '/^log_file:/{print $$2}' /etc/frostd.yaml 2>/dev/null); \
	sudo rm -f /etc/frostd.yaml; \
	if [ -n "$$LOG_FILE" ]; then sudo rm -f "$$LOG_FILE"; fi

.PHONY: package
package: build
	packaging/build-deb.sh
