# PKGBUILD for linux-power-tray
pkgname=linux-power-tray
pkgver=1.0.0
pkgrel=1
pkgdesc="System tray utility to toggle CPU frequency governors on Linux"
install=linux-power-tray.install
arch=('x86_64' 'aarch64' 'armv7h')
license=('GPL3')
depends=('qt6-base' 'ryzenadj')
makedepends=('cmake')
source=(
    'main.cpp'
    'set-governor.cpp'
    'CMakeLists.txt'
    'linux-power-tray.desktop'
    'linux-power-tray.install'
)
sha256sums=('SKIP' 'SKIP' 'SKIP' 'SKIP' 'SKIP')

build() {
    cmake -B build -S "$srcdir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    DESTDIR="$pkgdir" cmake --install build
    # Set the setuid bit so any user can run the helper as root.
    # The binary validates all input against a strict whitelist before
    # touching sysfs — this is the standard Unix pattern (cf. passwd, ping).
    chmod 4755 "$pkgdir/usr/lib/linux-power-tray/set-governor"
}
