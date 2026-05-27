BINARY = frostd
GOLANGCI_LINT = ./bin/golangci-lint
GOLANGCI_LINT_VERSION = v2.11.4

.PHONY: build
build:
	go build -o $(BINARY) .

.PHONY: run
run:
	go run . -c dev.yaml

.PHONY: test
test:
	go test -race ./...

$(GOLANGCI_LINT):
	curl -sfL https://raw.githubusercontent.com/golangci/golangci-lint/master/install.sh | sh -s -- -b ./bin $(GOLANGCI_LINT_VERSION)

.PHONY: lint
lint: $(GOLANGCI_LINT)
	$(GOLANGCI_LINT) run

.PHONY: install
install: build
	test -f /etc/frostd.yaml || sudo cp frostd.yaml /etc/frostd.yaml
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
