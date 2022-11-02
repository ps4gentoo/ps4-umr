# Original author: Emil Velikov <emil.l.velikov@gmail.com>
# Maintainer: Dmitrii Galantsev <dmitriigalantsev@gmail.com>

pkgname=umr
epoch=1
pkgver=1.0.0
pkgrel=1
pkgdesc='userspace debugging and diagnostic tool for AMD GPUs using the AMDGPU kernel driver'
arch=('i686' 'x86_64')
url='https://lists.freedesktop.org/archives/amd-gfx/2017-February/005122.html'
license=('MIT')
depends=('libpciaccess' 'ncurses' 'llvm-libs')
makedepends=('git' 'cmake' 'llvm' 'libdrm')
provides=('umr')
conflicts=('umr')
source=('umr.tar')
sha256sums=('SKIP')

pkgver() {
	git describe --tags
}

build() {
	cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_BUILD_TYPE=Release -DUMR_NO_GUI=ON .
}

package() {
	make DESTDIR="$pkgdir" install
	install -Dt "$pkgdir/usr/share/licenses/$pkgname" -m644 LICENSE
}
