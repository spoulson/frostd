BINARY = frostd

.PHONY: build
build:
	go build -o $(BINARY) .

.PHONY: run
run:
	go run . -c dev.yaml

.PHONY: test
test:
	go test ./...

.PHONY: install
install: build
	sudo cp frostd.yaml /etc/frostd.yaml
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

.PHONY: package
package: build
	packaging/build-deb.sh
