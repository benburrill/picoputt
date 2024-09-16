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
Note: I use units of $\hbar = 1$, so if the units don't make sense, hopefully it's because of that and not because I'm a klutz.

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
We just need to perform staggered updates to the real and imaginary components of the wavefunction:

```python
while True:
    real += dt * H(imag)
    imag -= dt * H(real)
```

(As with other staggered methods, you should treat `real` and `imag` as staggered in time by half a timestep).

<details>
<summary>Stability of the Visscher algorithm (click for details)</summary>

> For $\hat{H} = \frac{\hat{p}^2}{2m}$ and grid spacing $\mathtt{dx}$, Visscher's algorithm is stable for $\mathtt{dt}\leq{}m\mathtt{dx}^2$.
> Stability is also affected by the choice of potential.
> For best results, you may need to level-shift your potential so that as much as possible is close to 0.
> See [^visscher1991] for more details and derivations.
> 
> For a 2D grid, I found that a 9-point stencil is necessary for the Laplacian in order to reach the theoretical stability bounds
> (with a 5-point stencil, the stability region shrinks by a factor of 2).
</details>

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
Energy dissipation is an essential feature of the game.
Picoputt uses something I call "phase drag", which is a quantum analog to the linear drag force $F=-bv$.
Phase drag is not the *only* way to construct a quantum analog to the drag force,
and certainly not the only way to get some kind of dissipative effect,
but of the possibilities I considered, I believe it is the best.

The basic idea is to use the spatial phase gradient, $\nabla{}\theta(\vec{x})$ of a wavefunction
$\Psi(\vec{x}) = r(\vec{x})e^{i\theta(\vec{x})}$ as a measure of local momentum for the drag force to act on.

Considering only the effect of the drag force, in one time step $\Delta{}t$,
if linear drag would scale a particle's momentum by a factor of $\alpha=e^{-b{}\Delta{}t/m}$,
then phase drag should likewise scale the phase gradient everywhere,
transforming $\Psi$ to $r(\vec{x})e^{i(\alpha\theta(\vec{x}) + C)}$
with any constant $C$ (which contributes an unobservable global phase shift).

<details>
<summary>The expectation value of momentum is equal to the expectation value of the spatial phase gradient (click for math).</summary>

> ```math
> {\left\langle{}p_x\right\rangle} = \int_{-\infty}^{\infty} re^{-i\theta}\left(-i \frac{d}{dx}\right)re^{i\theta} dx = -i\int_{r_{-\infty}}^{r_{\infty}} r dr + \int_{-\infty}^{\infty} \frac{d\theta}{dx} {\left\lvert\Psi\right\rvert}^2 dx = 0 + \left\langle\frac{d\theta}{dx}\right\rangle
> ```
</details>

So, to verify that phase drag acts like linear drag on the expectation value of momentum, we can simply observe that when we
rescale the phase gradient, $\left\langle{}p\right\rangle = \left\langle\nabla{}\theta\right\rangle = \left\langle\alpha\nabla{}\theta_0\right\rangle = \alpha\left\langle{}p_0\right\rangle$.

Equivalently, we can define a drag potential $V_{drag} = \frac{b}{m}\theta{}(\vec{x})$, which can be added to the Hamiltonian.

<details>
<summary>Admittedly, by defining $\theta{}(\vec{x})$, I have concealed some mathematical ambiguities (click for the ugly truth).</summary>

> The complex logarithm is multivalued, so for any wavefunction, there are infinitely many $\theta{}(\vec{x})$ functions to choose from.
> Although we can unambiguously determine $\nabla{}\theta$ everywhere (ignoring nodes), it is not always possible for $\theta$ to be continuous.
> This occurs in the case of quantum vortices/eigenstates of angular momentum (more concretely, think of $\nabla{}\theta$ pointing "tangentially along the orbit").
> In such cases, for $\Psi$ to be continuous, $\theta$ must have a $2\pi{}n$ discontinuity.
> So (except for certain quantized increments), if we attempt to rescale $\nabla{}\theta$, then $\Psi$ will inevitably get a discontinuity *somewhere*,
> and worse, the location of this discontinuity is not even well-defined!
> 
> <details>
> <summary>Non-rotational nodes *seem* like they also should be a problem, even in 1-D: the phase gradient is infinite! (click)</summary>
> 
>> But this isn't really a problem we need to worry about.  Instead, think of almost-nodes with very large phase gradients
>> which jump easily (with a tiny perturbation) between positive and negative.  The "correct" behavior for true nodes in 1-D
>> is probably to randomly choose $\pm\frac{\pi}{\mathtt{dx}}$, but in practice almost-nodes are the typical case, so whatever
>> arbitrary choice we make for true nodes hardly matters.
> </details>
</details>

The simplest way to deal with the problem of vortices is to somewhat arbitrarily say that the drag force only acts on the irrotational component of the phase gradient.
More formally, we can define $V_{drag}$ as a solution to the Poisson equation $\nabla^2{}V_{drag} = b\nabla{}\cdot(\nabla{}\theta)$
satisfying appropriate boundary conditions (which are a bit messy since $\theta$ is undefined at the boundary).

However, in picoputt, I do not actually solve this equation.  Instead, I use a fast and loose algorithm that mostly kinda works to approximate $V_{drag}$.
See the section on [LIP integration](#lip-integration) for more details.

TODO: discuss some interesting properties of phase drag

### LIP integration
LIP integration is an unconventional (seemingly novel?)
highly parallelizable non-iterative multi-scale method I designed for picoputt to determine the drag potential,
but more generally it could be used to find a scalar potential for any vector field.
It is exact (up to numerical error) in the case of a conservative field,
and non-conservative features (in particular, point vortices of the kind that occur in phase gradients)
have a reasonably small effect on the potential.

The basic idea is pretty simple:
since the problem with non-conservative fields is essentially that
different path integrals between 2 points can produce different results,
we'll do some sort of weighted average of a whole bunch of path integrals to smooth over any inconsistencies.

The algorithm constructs a multi-scale line integral pyramid (the eponymous LIP), recursively using locally-averaged line integrals
from the previous layer to determine the line integrals between points twice as distant.
At the top layer of the pyramid, we have 4 line integrals, one for each edge of the rectangular grid,
each one (in some way) incorporating every vector in the field.

From the top layer of the pyramid, we can determine values of the scalar potential for the 4 corners of the grid.
From there we fill in the interior points in a "bilinear-ish" way, descending the pyramid to get the relevant line integrals.
To illustrate better, here's an example of the order in which points get filled in for a small grid:

```math
\begin{matrix}
0 & 3 & 1 & 3 & 0 \\
4 & 4 & 4 & 4 & 4 \\
2 & 3 & 2 & 3 & 2 \\
4 & 4 & 4 & 4 & 4 \\
0 & 3 & 1 & 3 & 0
\end{matrix}
```

From the skeleton of the algorithm I've laid out so far, for a grid with $n$ points, the sequential time complexity is $O(n)$,
and when parallelized, there are $O(\log(n))$ stages (same as parallel prefix sum).
By contrast, in a more conventional "full-multigrid" iterative relaxation algorithm
(see for example Pritt 1996[^pritt1996]),
although each FMG cycle also has $O(n)$ sequential time complexity, when parallelized they require $O(\log(n)^2)$ stages.
Many other alternatives (eg FFT-based approaches) also fall short in either sequential or parallel complexity.

So at least in theory, with sufficiently large grid sizes, on a GPU with sufficiently many cores,
our algorithm should be faster (at the cost of potentially undesirable results for non-conservative fields).
We just need to find a weighting scheme of nearby path integrals in the pyramid that minimizes the amount of jankiness
to a level undetectable by the player.

The simplest case is the $2^k + 1$ grid sizes, as those can be perfectly subdivided.
I found a good weighting scheme for these $2^k + 1$ grids quite quickly:  
![Diagram showing weighting scheme which works well for 2^k + 1 grids](https://github.com/user-attachments/assets/c764f3f0-a8aa-44c2-8300-016030ae53d5)

As illustrated in the diagram above, the line-integrals of the next layer are a weighted average of up to 3 paths:
the straight-line path
(which is the sum of 2 line-integrals from the previous layer, or 1 vector from the field for the bottom layer of the pyramid)
as well as 2 square "lobes" (or 1 lobe if we are at the edge of the grid).
The lobes have weight $\frac{1}{4}$, and the straight-line path gets the remaining weight.
Of course, each of the so-called "line-integrals" from the previous layer are themselves weighted averages of many paths.

By virtue of path independence, no matter what weights we choose, the algorithm will be exact for any conservative field.

In the case of a $2\pi$ complex point vortex (which has a non-conservative phase gradient, which we aim to eliminate),
LIP-integration produces the following result (grid size is $513\times{}513$, results are similar for any square $2^k + 1$ grid):

![Plot comparing the phase of a central complex point vortex with the LIP-integration of its phase gradient](https://github.com/user-attachments/assets/21dd0237-d8b8-40db-bf2e-88765f7eb206)

This is a fairly good result.
This central vortex leaves behind only a small artifact on the reconstructed scalar potential,
with extremes of $\pm{}(\arctan(1/2)-\arctan(1)/2) \approx{} \pm{}0.0709$, or about 2% of $\pi{}$.
The effect is also spread out in a fairly even and radially symmetric way with no sharp discontinuities.

Encouraged by this early success (and hypnotized by the pretty fractal patterns),
I set out on a futile and somewhat pointless quest to "correctly" generalize the algorithm for other grid sizes.

Long story short, after an embarrassingly long time trying to generalize the algorithm in a sensible way
(and trying to understand better what my algorithms were even doing mathematically),
I had to throw in the towel as I was getting nowhere.
Whenever I felt I had a promising idea, it was crap, but random nonsense tweaks sometimes improved things.
However, despite being riddled with undesirable properties, most of my attempts seemed to be good enough for the purpose of the game at least.

The generalization picoputt uses I picked because it's relatively easy to implement and seems to give decent results for many grid sizes,
despite making basically no sense whatsoever.  It does not merit more explanation here,
but if you want to know how the sausage is made, see [the code](shaders/drag).

### Measurement
TODO: explain what I'm doing, and what parts of it are weird/wrong.

TODO: explain problem of discontinuities at walls - heat kernels as solution?

### Winning
The winning "hole state" is supposed to be the local minimum energy state.  So what do I actually mean by that?  
Honestly, I'm not sure exactly.  But no matter: approximate understanding ought to be good enough for an approximation.
Intuitively, we should expect that the ground state of a good local approximation of the Hamiltonian will be a good
approximation of the local minimum energy state.  And we can tell it works if when you put the particle into that state
it stays put, even when subject to the drag force.

I currently place a cosine potential well at the holes.  Locally, we can approximate this as a harmonic oscillator,
for which the ground state is a Gaussian.

### Putt wave
When the player putts, we essentially want to apply some impulse $\Delta{}\vec{p}$ to the wavefunction.
The simplest way to do this would be to multiply the wavefunction with the plane wave $\exp\left({i\Delta{}\vec{p}\cdot\vec{x}}\right)$.
However, to give the player some more control, I wanted to limit the effect of the putt to some circular region of radius $r$.

To accomplish this, I chose (for $\Delta{}\vec{p}$ along $\hat{x}$) the function:

```math
\exp\left({i\Delta{}pr\arctan\left(\frac{x}{\sqrt{y^{2}+r^{2}}}\right)}\right)
```

In appearance, the real component is similar to (but distinct from) the interference pattern of 2 point sources.

Here's a desmos plot of the putt wave to play around with: https://www.desmos.com/3d/yvy4xhqngx

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

[^visscher1991]: PB Visscher. A fast explicit algorithm for the time-dependent Schrödinger equation. Computers in Physics. https://doi.org/10.1063/1.168415. 1991.
[^pritt1996]: MD Pritt. Phase Unwrapping by Means of Multigrid Techniques for Interferometric SAR. IEEE. https://doi.org/10.1109/36.499752. 1996
