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
* **Click and drag to putt.**  The strength of the putt is determined by the distance between where you click and where
  you release the mouse.  Press Escape to cancel the putt.
* **Scroll up and down** (or use `[` and `]`) to adjust the **radius of the putt-wave**.  This changes the area of
  effect of the putt.
* **Spacebar to measure position**.  This will re-localize the particle.
* `R` to restart.
* `P` to pause/unpause.
* Curios: `D` for debug views, `M` to simulate 100 measurements.

As in classical golf, lower score is better, and putting adds 1 to your score.  
Measurement also adds to your score, but it only adds 1/2.  
*In other words: measure twice, putt once!*

To win, you need at least **50% probability** of finding the particle in the **local minimum energy state at the goal hole.**
The hole itself is a potential well which will attract the particle.  The local minimum energy state is referred to as
the "hole state" $\left\lvert{\rm hole}\right\rangle$ and the current probability is displayed in game as $P({\rm win})$.

The location of **the goal hole is indicated with a green arrow**.  When the ball is in the hole state, its probability
distribution will look like a **stationary round bump** centered at this location.  
*It may take some time for the drag force to slow the ball down enough to reach sufficient probability in this
low-energy state.*

However, you may find that rather than a round bump, you instead get "divots" in the ball's probability distribution
which never go away and prevent you from winning.  These are **quantum vortices**: annoying little quantized bundles of
angular momentum, which are not directly affected by the drag force.  If you're having trouble dealing with vortices,
measurement can be useful tool to get rid of them.

![Image illustrating the appearance of a quantum vortex in comparison to the hole state](https://github.com/user-attachments/assets/7e0d6d4d-3661-44c7-84b8-be53e044b7ba)

By adjusting the radius of the putt-wave, you can change how localized the effect of putt on the momentum of the golf
ball is.  The biggest and most uniform effect on momentum is inside this radius (where the de Broglie wavelength of the
putt-wave is shortest and looks approximately like a plane wave).  Outside of this radius, the effect on local momentum
is small, but the distortion of the putt-wave can be useful to compress, disperse, or otherwise manipulate the shape of
a wavepacket.

If you need an extra challenge, here are some things you can try:
* Win without measurement
* Get a hole in 1
* Get a hole in 1/2 (very luck-based)


## Technical details
Note: I use units of $\hbar = 1$, so if ever the units don't make sense, hopefully it's because of that and not because I'm a klutz.

### Quantum simulation
The time-dependent Schrödinger equation, which describes the evolution of a quantum particle, can be written as

```math
\frac{d{}\Psi}{dt} = -i\hat{H}{}\Psi
```

Where $\Psi$ is the wavefunction and $\hat{H}$ is the linear Hamiltonian operator describing the system.  
The most obvious way we might think of to simulate this is to directly apply Euler's method:

```python
while True:
    psi -= dt * 1j * H(psi)
```

Unfortunately, this method is unstable!  It will always blow up, no matter how small you choose `dt`.

Thankfully, there is a small tweak we can do (described in Visscher 1991[^visscher1991]) which affords us a region of stability.  
We just need perform staggered updates to the real and imaginary components of the wavefunction:

```python
while True:
    real += dt * H(imag)
    imag -= dt * H(real)
```

TODO: talk about stability, level shifts

For a 2D grid, I found that a 9-point stencil is necessary for the Laplacian in order to reach the theoretical stability bounds
(with a 5-point stencil, the stability region shrinks by a factor of 2).

To simplify writing the algorithm with a single shader to run on the GPU,
I actually implement the Visscher algorithm slightly differently from this, using what I call a "qturn" (short for quarter turn).
A qturn is a single Euler step of the real component of the wavefunction followed by a complex rotation of 90 degrees.
4 qturns is exactly equivalent to 2 iterations of the Visscher algorithm, so we can simply iteratively apply qturns to update the wavefunction, ie:

```python
while True:
    real, imag = -imag, real + dt * H(imag)
```

Of course this is just a minor implementation detail, but I emphasize it because it affects the terminology I use.
To keep the global phase consistent, picoputt always performs its physics updates in batches of 4 qturns,
which I refer to as physics "turns" to distinguish them from timesteps.  So to be clear, 1 "turn" = 4 "qturns" = 2 "timesteps".

The drag force is also only updated once per turn.
The "max perf" stat shown in-game is an estimate of the maximum number of turns (4 qturns + 1 drag update)
which could be run in one second on your machine (based on the average time taken to run each turn).

The code can be found in [qturn.frag](shaders/qturn.frag).


### Quantum drag force
Energy dissipation is an essential feature of the game.  Picoputt uses a quantum analogue for a linear drag force ($F=-bv$), which I call "phase drag".
It is not the *only* way to get something that acts like a drag force, and certainly not the only way to get some kind of dissipative effect,
but of the possibilities I considered, I believe it is the best.

The idea is to use the spatial phase gradient, $\nabla{}\theta(\vec{x})$ of a wavefunction $\Psi(\vec{x}) = r(\vec{x})e^{i\theta(\vec{x})}$
as a measure of local momentum for the drag force to act on.
If in one time step $\Delta{}t$, linear drag would scale a particle's momentum by a factor of $\alpha=e^{-b{}\Delta{}t/m}$,
then phase drag should likewise scale the phase gradient, transforming $\Psi$ to $r(\vec{x})e^{i(\alpha\theta(\vec{x}) + C)}$
with any constant $C$ (which contributes an unobservable global phase shift).

<details>
<summary>The expectation value of momentum is equal to the expectation value of the spatial phase gradient (click for math).</summary>

```math
{\left\langle{}p_x\right\rangle} = \int_{-\infty}^{\infty} re^{-i\theta}\left(-i \frac{d}{dx}\right)re^{i\theta} dx = -i\int_{r_{-\infty}}^{r_{\infty}} r dr + \int_{-\infty}^{\infty} \frac{d\theta}{dx} {\left\lvert\Psi\right\rvert}^2 dx = 0 + \left\langle\frac{d\theta}{dx}\right\rangle
```
</details>

So, to verify that phase drag acts like linear drag on the expectation value of momentum, we can simply observe that when we
rescale the phase gradient, $\left\langle{}p\right\rangle = \left\langle\alpha\nabla{}\theta_0\right\rangle = \alpha\left\langle{}p_0\right\rangle$.

Another way to think about this is to define a drag potential $V_{drag} = b\theta{}(\vec{x})$, which we can add to the Hamiltonian.

Admittedly, by defining $\theta{}(\vec{x})$, I have concealed some ugly mathematical ambiguities.
The complex logarithm is multivalued, so for any wavefunction, there are infinitely many $\theta{}(\vec{x})$ functions to choose from.
Although we can unambiguously determine $\nabla{}\theta$ everywhere (ignoring nodes), it is not always possible for $\theta$ to be continuous if there are periodic boundary conditions.
This occurs in the case of quantum vortices/eigenstates of angular momentum (more concretely, think of $\nabla{}\theta$ pointing "tangentially along the orbit").
In such cases, for $\Psi$ to be continuous, $\theta$ must have a $2\pi{}n$ discontinuity.
So (except for certain quantized increments), if we attempt to rescale $\nabla{}\theta$, then $\Psi$ will inevitably get a discontinuity *somewhere*,
and worse, the location of this discontinuity is not even well-defined!

The simplest way to deal with this problem is to somewhat arbitrarily say that the drag force only acts on the irrotational component of the phase gradient.
More formally, we can define $V_{drag}$ as a solution to $\nabla^2{}V_{drag} = b\nabla{}\cdot(\nabla{}\theta)$ satisfying appropriate boundary conditions
(which are a bit messy since $\theta$ is undefined at the boundary).

However, in picoputt, I do not actually solve this equation.  Instead I use a fast and loose algorithm that mostly kinda works to approximate $V_{drag}$.
See the section on [LIP integration](#lip-integration) for more details.

### LIP integration
The basic idea is pretty simple: since the problem with non-conservative fields is essentially that different path integrals between 2 points can produce different results,
we'll do some sort of weighted average of a whole bunch of path integrals to smooth over any inconsistencies.

The algorithm constructs a multi-scale line integral pyramid (the eponymous LIP), recursively using locally-averaged line integrals
from the previous level to determine the line integrals between points twice as distant.
At the top level of the pyramid, we have 4 line integrals, one for each edge of the rectangular grid,
each one incorporating every discrete difference from the bottom level.

From the top level of the pyramid, we can determine values of the scalar potential for the 4 corners of the grid.
From there we fill in the interior points in a "bilinear-ish" way, descending the pyramid to get the relevant line integrals.
To illustrate better, here's an example of the order in which points get filled in:

```math
\begin{matrix}
0 & 3 & 1 & 3 & 0 \\
4 & 4 & 4 & 4 & 4 \\
2 & 3 & 2 & 3 & 2 \\
4 & 4 & 4 & 4 & 4 \\
0 & 3 & 1 & 3 & 0
\end{matrix}
```

For a grid with $n$ points, from the skeleton of the algorithm I've laid out so far, the sequential time complexity is $O(n)$,
and when parallelized, there are $O(\log(n))$ stages.
A "full-multigrid" iterative relaxation algorithm would also have $O(n)$ sequential time complexity, but would require $O(\log(n)^2)$ stages.
So at least in theory, with sufficiently large grid sizes, on a GPU with sufficiently many cores, this algorithm should be faster.
We just need to find a weighting scheme for nearby path integrals that produces acceptable results in our pyramid.

The simplest case is the $2^k + 1$ grid sizes, as those can be perfectly subdivided.
I found a good weighting scheme for these $2^k + 1$ grids quite quickly.

TODO: explain with plots and diagrams, arctan(1/2)-arctan(1)/2 = arctan(1/2)-pi/8 = 0.0709 = ~2% of pi


Encouraged by this early success (and hypnotized by the pretty fractal patterns),
I set out on a futile and somewhat pointless quest to "correctly" generalize the algorithm for other grid sizes.

Long story short, after a embarrassingly long time trying to generalize the algorithm
(and trying to understand better what my algorithms were even doing mathematically),
I had to throw in the towel as I was getting nowhere.
Whenever I felt I had a promising idea, it was crap, but random nonsense tweaks sometimes improved things.
However, despite being riddled with undesirable properties, most of my attempts seemed to be good enough for the purpose of the game at least.

The generalization picoputt uses I picked because it's relatively easy to implement and seems to give decent results for many grid sizes,
despite making basically no sense whatsoever.  It does not merit more explanation here,
but if you want to know how the sausage is made, see [the code](shaders/drag).

### Measurement
TODO: explain what I'm doing, and what parts of it are weird

TODO: explain problem of discontinuities at walls - heat kernels as solution?

### Local minimum energy state
The winning "hole state" is an approximation of the local minimum energy state, obtained by finding the ground state of a local approximation of the Hamiltonian at the hole.

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

[^visscher1991]: PB Visscher. A fast explicit algorithm for the time-dependent Schrödinger equation. https://doi.org/10.1063/1.168415. Computers in Physics, 5(6):596–598, 1991.
