# Maintainer: Emil Velikov <emil.l.velikov@gmail.com>
# Maintainer: Dmitrii Galantsev <dmitriigalantsev@gmail.com>

pkgname=umr
pkgver=1.0.9
pkgrel=1
pkgdesc='User Mode Register Debugger for AMDGPU Hardware'
arch=('i686' 'x86_64')
url='https://gitlab.freedesktop.org/tomstdenis/umr'
license=('MIT')
depends=('libpciaccess' 'ncurses' 'llvm-libs' 'sdl2' 'nanomsg')
makedepends=('cmake' 'llvm' 'libdrm')
source=("$pkgname-$pkgver.tar.bz2")
sha256sums=('SKIP')
options=(!debug)

build() {
	local cmake_args=(
		-B build -S .
		-DCMAKE_INSTALL_PREFIX=/usr
		-DCMAKE_INSTALL_LIBDIR=lib
		-DCMAKE_BUILD_TYPE=Release
		-DUMR_INSTALL_DEV=ON
		-DUMR_INSTALL_TEST=ON
	)

	cmake "${cmake_args[@]}"
	cmake --build build
}

package() {
	DESTDIR="$pkgdir" cmake --install build
	install -Dt "$pkgdir/usr/share/licenses/$pkgname" -m644 "LICENSE"
}
