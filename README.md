# Picoputt
Taking the concept of miniature golf to its logical extreme, in picoputt your golf ball is a quantum particle.

Features:
* Fast, GPU-accelerated simulation of the time-dependent Schrödinger equation.
* Energy dissipation with quantum analog to a linear drag force.
* Putting: hit the quantum golf-ball around using a golf club "putt-wave" with adjustable radius.
* Simultaneous partial measurement of position and momentum.
* Play golf, not dice: Although the randomness of measurement can be helpful, you can win with putting alone (which is not random).
* Fancy pseudo-3D graphics


## Obtaining picoputt
You can download releases of picoputt for Windows or Linux [here](https://github.com/benburrill/picoputt/releases).  
MacOS is sadly **not supported** (because picoputt requires OpenGL 4.3 for compute shaders).

If you wish to compile picoputt from source, see the [build instructions](#build-instructions) below.


## How to play
Controls:
* **Click and drag to putt.**  The strength of the putt is determined by the distance between where you click and where you release the mouse.
* **Scroll up and down** (or use `[` and `]`) to adjust the **radius of the putt-wave**.  This changes the area of effect of the putt.
* **Spacebar to measure position**.  This will re-localize the particle.
* `R` to restart.
* `P` to pause/unpause.

As in classical golf, lower score is better, and putting adds 1 to your score.  
Measurement also adds to your score, but it only adds 1/2.  
*In other words: measure twice, putt once!*

To win, you need at least **50% probability** of finding the particle in the **local minimum energy state at the goal hole.**
The hole itself is a potential well which will attract the particle.  The local minimum energy state is referred to as
the "hole state" $\lvert{\rm hole}\rangle$ and the current probability is displayed in game as $P({\rm win})$.

The location of **the goal hole is indicated with a green arrow**.  When the ball is in the hole state, its probability
distribution will look like a **stationary round bump** centered at this location.  
*It may take some time for the drag force to slow the ball down enough to reach sufficient probability in this
low-energy state.*

However, you may find that rather than a round bump, you instead get "divots" in the ball's probability distribution
which never go away and prevent you from winning.  These are **quantum vortices**: annoying little quantized bundles of
angular momentum, which are not directly affected by the drag force.  If you're having trouble dealing with vortices,
measurement can be useful tool to get rid of them.

![vortex](https://github.com/user-attachments/assets/7e0d6d4d-3661-44c7-84b8-be53e044b7ba)

By adjusting the radius of the putt-wave, you can change how localized the effect of putt on the momentum of the golf
ball is.  The biggest and most uniform effect on momentum is inside this radius (where the de Broglie wavelength of the
putt-wave is shortest and looks approximately like a plane wave).  Outside of this radius, the effect on local momentum
is small, but the distortion of the putt-wave can be useful to compress, disperse, or otherwise manipulate the shape of
a wavepacket.

If you need an extra challenge, here are some things you can try:
* Win without measurement
* Get a hole in 1
* Get a hole in 1/2 (very luck-based)


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
