# Picoputt
Taking the concept of miniature golf to its logical extreme, in picoputt your golf ball is a quantum particle.

Currently, it is more of an interactive simulation/toy rather than a game, as there's not yet a way to win.

However, the following features have been implemented:
* Fast, GPU-accelerated simulation of the time-dependent Schrödinger equation.
* Putting: hit the quantum golf-ball around using a golf club "putt-wave" with adjustable radius.
* Measurement of position (though it's currently not implemented as a proper partial measurement).
* Energy dissipation with quantum analog to a linear drag force.
* Visualization with fancy pseudo-3D graphics:

https://github.com/benburrill/picoputt/assets/8140726/2fdb1340-4eff-421d-90ee-8a88f483e5da


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
Click and drag to putt.

Scroll up and down (or use `[` and `]`) to adjust the radius of the putt-wave.

*The de Broglie wavelength of the putt-wave is shortest (and is approximately a plane wave) inside this radius, so that
is where you will have the biggest and most uniform effect on the momentum of the particle.  Outside of this radius, the
effect on local momentum is small, but the distortion of the putt-wave can be useful to compress, disperse, or otherwise
manipulate the shape of a wavepacket.*

Press space to measure position.  Although it's intended to resemble a partial measurement, currently the
post-measurement state always has $\langle{}p\rangle = 0$ which isn't very realistic.

Here's an example of pulling apart some quantum taffy and measuring its position:

https://github.com/benburrill/picoputt/assets/8140726/d74da0ca-a8cb-436b-91cf-6a74b47e99d9


## Goals
Contrary to the source material, I don't want picoputt to be too much of a game of chance -- god might play dice, but
we're trying to play golf.  Although there will be (partial) position measurement, it won't be a matter of measuring
the golf ball's position until you get lucky enough for it to end up in the hole, measurement will just be a tool to
re-localize the golf ball that adds to your score, just like putting will.
I plan to make putting contribute 1 to your score (like classical golf) and measurement contribute 1/2.  In other words:
measure twice, putt once (not necessarily in that order).

The object of the game will be to reach some probability threshold in a particular energy eigenstate of the golf course,
specifically the local minimum energy state at the hole.  Since it will be based on energy state rather than position,
simply getting probability into the hole won't be sufficient -- the ball needs to come to rest there.  The local minimum
energy state at the hole of the total golf-course Hamiltonian $H_g = H_0 + V_{hole}$ will be approximated by simply
using the ground state of the hole Hamiltonian, but in order for the
ball to come to rest, a dissipative effect is necessary.

Energy dissipation in picoputt is implemented with an effect I call phase drag, which acts as a linear drag force on
the phase gradient.  I'll describe more technical details later, but basically phase gradient is a measure of local
momentum, and so phase drag tries to decrease the phase gradient everywhere each timestep.  Actually the irrotational
component of the phase gradient is what's used, so phase drag has no effect on quantum vortices.  Although it's a bit
mathematically sketchy, qualitatively it works quite well as a drag force and has some interesting effects like
decreasing the tendency of a wavepacket to spread out (making it "more classical").
