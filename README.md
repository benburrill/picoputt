# Picoputt
Taking the concept of miniature golf to its logical extreme, in picoputt your golf ball is a quantum particle.

Features:
* Fast, GPU-accelerated simulation of the time-dependent Schrödinger equation.
* Energy dissipation with quantum analog to a linear drag force.
* Putting: hit the quantum golf-ball around using a golf club "putt-wave" with adjustable radius.
* Measurement of position (though it's currently not implemented as a proper partial measurement).
* Play golf, not dice: Although the randomness of measurement can be helpful, you can win with putting alone (which is not random).
* Fancy pseudo-3D graphics


## Build instructions
Picoputt depends on SDL2 and GLEW.  I install them with vcpkg: `vcpkg install sdl2 glew`.

Build with CMake and run:
```shell
$ cmake .  # If using vcpkg, add -DCMAKE_TOOLCHAIN_FILE=...
$ cmake --build .
$ ./picoputt
```

By default, picoputt looks for resources in its parent directory.  If you want to use a different build directory, since
these files are not copied over to the build directory, you may want to override the base directory using the
`$PICOPUTT_BASE_PATH` environment variable:
```shell
$ cmake --build builddir
$ PICOPUTT_BASE_PATH=. ./builddir/picoputt
```

You can also create a zip package with all necessary components using
`cmake --build builddir -- package`


## How to play
To win, you need to reach a certain probability threshold of finding the
particle in the local minimum energy state of the goal hole.

This local minimum energy state will look like a stationary round bump
at the location of the hole.  You may need to be patient while the drag
force slows the ball down enough to reach sufficient probability in this
low-energy state.  However, you may encounter "dents" which never go
away, preventing you from winning.  These are quantum vortices: annoying
little quantized bundles of angular momentum, which are not directly
affected by the drag force.  If you're having trouble dealing with
vortices, measurement can be useful tool to get rid of them.

Controls:
* Click and drag to putt.
* Scroll up and down (or use `[` and `]`) to adjust the radius of the putt-wave.
* Press space to measure position.  Although it's intended to resemble a partial measurement, currently the
  post-measurement state always has $\langle{}p\rangle = 0$ which isn't very realistic.
* Press `R` to restart

By adjusting the radius of the putt-wave, you can change how localized the effect of putt on the momentum of the golf
ball is.  The biggest and most uniform effect on momentum is inside this radius (where the de Broglie wavelength of the
putt-wave is shortest and looks approximately like a plane wave).  Outside of this radius, the effect on local momentum
is small, but the distortion of the putt-wave can be useful to compress, disperse, or otherwise manipulate the shape of
a wavepacket. 

As in classical golf, putting adds 1 to your score.  Measurement also adds to your score, but it only adds 1/2.
In other words: measure twice, putt once!
