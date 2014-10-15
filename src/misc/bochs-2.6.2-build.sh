#! /bin/sh -e

if test -z "$SRCDIR" || test -z "$PINTOSDIR" || test -z "$DSTDIR"; then
    echo "usage: env SRCDIR=<srcdir> PINTOSDIR=<srcdir> DSTDIR=<dstdir> sh $0"
    echo "  where <srcdir> contains bochs-2.6.2.tar.gz"
    echo "    and <pintosdir> is the root of the pintos source tree"
    echo "    and <dstdir> is the installation prefix (e.g. /usr/local)"
    exit 1
fi

cd /tmp
mkdir $$
cd $$
mkdir bochs-2.6.2
tar xzf $SRCDIR/bochs-2.6.2.tar.gz
cd bochs-2.6.2
cat $PINTOSDIR/src/misc/bochs-2.6.2-jitter-plus-segv.patch | patch -p1
cat $PINTOSDIR/src/misc/bochs-2.6.2-xrandr-pkgconfig.patch | patch -p1
CFGOPTS="--with-x --with-x11 --with-term --with-nogui --prefix=$DSTDIR --enable-cpu-level=6"
mkdir plain &&
        cd plain && 
        ../configure $CFGOPTS --enable-gdb-stub && 
        make && 
        make install &&
        cd ..
mkdir with-dbg &&
        cd with-dbg &&
        ../configure --enable-debugger $CFGOPTS &&
        make &&
        cp bochs $DSTDIR/bin/bochs-dbg &&
        cd ..
