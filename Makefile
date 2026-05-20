BINARY = frostd

.PHONY: build run test install uninstall package

build:
	go build -o $(BINARY) .

run:
	go run . -c dev.yaml

test:
	go test ./...

install: build
	sudo cp frostd.yaml /etc/frostd.yaml
	sudo sed "s|ExecStart=.*|ExecStart=$(CURDIR)/$(BINARY)|" \
	    packaging/frostd.service \
	    | sudo tee /etc/systemd/system/frostd.service > /dev/null
	sudo systemctl daemon-reload
	sudo systemctl enable frostd
	sudo systemctl start frostd

uninstall:
	sudo systemctl stop frostd || true
	sudo systemctl disable frostd || true
	sudo rm -f /etc/systemd/system/frostd.service
	sudo systemctl daemon-reload

package: build
	packaging/build-deb.sh
