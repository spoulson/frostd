#!/bin/sh
set -e

BINARY=frostd
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "0.0.0")
PKG=pkg

rm -rf "$PKG"
mkdir -p "$PKG/DEBIAN"
mkdir -p "$PKG/usr/local/bin"
mkdir -p "$PKG/etc"
mkdir -p "$PKG/lib/systemd/system"

cp packaging/debian/control "$PKG/DEBIAN/control"
sed -i "s/^Version:.*/Version: $VERSION/" "$PKG/DEBIAN/control"

cp packaging/debian/postinst "$PKG/DEBIAN/postinst"
cp packaging/debian/prerm    "$PKG/DEBIAN/prerm"
chmod 755 "$PKG/DEBIAN/postinst" "$PKG/DEBIAN/prerm"

cp "$BINARY"             "$PKG/usr/local/bin/$BINARY"
cp frostd.yaml           "$PKG/etc/frostd.yaml"
cp packaging/frostd.service "$PKG/lib/systemd/system/frostd.service"

dpkg-deb --build "$PKG" "${BINARY}_${VERSION}_amd64.deb"
rm -rf "$PKG"

echo "Built: ${BINARY}_${VERSION}_amd64.deb"
