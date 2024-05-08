# Picoputt
Taking the concept of miniature golf to its logical extreme, in picoputt your golf ball is a quantum particle.

Currently, it is not so much an actual game as it is an interactive simulation/toy, and I'm still working on porting
some key features from my Python prototype (also not a complete game) to C/GLSL.

Currently, picoputt has the following features:
* Fast, GPU-accelerated simulation of the Schrödinger equation.  On my laptop's integrated GPU, it's ~60x faster than
  the equivalent Python/numpy implementation from my prototype.
* Putting: hit the quantum golf-ball around using a golf club "putt-wave" with adjustable radius.
* Measurement of position (though it's currently not implemented as a proper partial measurement).
* Visualization with fancy pseudo-3D graphics:

https://github.com/benburrill/picoputt/assets/8140726/2fdb1340-4eff-421d-90ee-8a88f483e5da


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

The object of the game will be to reach some probability threshold in a particular energy eigenstate of the golf course,
specifically the local minimum energy state at the hole.  Since it will be based on energy state rather than position,
simply getting probability into the hole won't be sufficient -- the ball needs to come to rest there.  For this to work,
we'll need a dissipative effect.  The plan is to use an effect I call phase drag, which acts as a linear drag force on
the phase gradient.
The local minimum energy state at the hole of the total golf-course Hamiltonian $H_g = H_0 + V_{hole}$ will be
approximated by simply using the ground state of the hole Hamiltonian.

I've yet to implement this phase-drag effect in picoputt, but I have implemented it in my prototype by using skimage's
unwrap_phase to do a 2D phase unwrapping, which I then scale down and reapply to the wavefunction's magnitude to
decrease the phase gradient everywhere each timestep.  Although it's a bit mathematically sketchy, qualitatively it
works quite well as a drag force and has some interesting effects like decreasing the tendency of a wavepacket to spread
out (making it "more classical").
