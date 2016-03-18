rm(list=ls())

# Costs from draw-perf (for stl)
#c_N <- 39.7798; c_U <- 10.5276; c_Exp <- 40.5834; c_ex <- 17.4958; c_sqrt <- 4.5262; a0 <- 0.7264; a0s <- 2.6948
# Costs from draw-perf (for boost)
c_N <- 12.0019; c_U <- 7.5485; c_Exp <- 33.3674; c_ex <- 17.4958; c_sqrt <- 4.5262; a0 <- 1.3618; a0s <- 2.4763
# Costs from draw-perf (for gsl-zigg)
#c_N <- 11.4305; c_U <- 6.9962; c_Exp <- 24.3831; c_ex <- 17.4958; c_sqrt <- 4.5262; a0 <- 1.2990; a0s <- 2.2527
# Costs from draw-perf (for gsl-rat)
#c_N <- 26.1273; c_U <- 6.9962; c_Exp <- 24.3831; c_ex <- 17.4958; c_sqrt <- 4.5262; a0 <- 0.7974; a0s <- 2.2527
# Costs from draw-perf (for gsl-BoxM)
#c_N <- 54.3344; c_U <- 6.9962; c_Exp <- 24.3831; c_ex <- 17.4958; c_sqrt <- 4.5262; a0 <- 0.2259; a0s <- 2.2527

# I could add timings for multiplications and additions, too, but when combined with other operations
# they often require no extra time.  For example, to compute:
#     0.5*(a + sqrt(a*a+4))
# appears (based on testing) to take exactly as long to compute as just sqrt(a), i.e. we
# are getting the other calculations essentially for free (at least on my Intel 2500K CPU),
# presumably because the CPU is able to perform the multiplications and additions using other
# execution units while the sqrt completes.  Even if this wasn't the case, however, they are
# extremely fast calculations relative to the above timings (around 19 times faster than c_sqrt).

a0_LG <- 0.2570
a0_Gw <- 0.45

# Line width:
lwd <- 1.5
# Colours and line styles
col <- rgb(0,0,0,1) # Our line
lty <- 1 # Solid
col_LG <- rgb(0,0,1,1) # Li and Ghosh
lty_LG <- 2
col_Gw <- rgb(1,0,0,1) # Geweke
lty_Gw <- 3
col_Ro <- rgb(0,0.5,0,1) # Robert
lty_Ro <- 4

# Shift the lines vertically by this amount for the older implementations to make them visible
# For precision, set this to F, for a less precise but easier to read graph, T
enable_vshift <- F
vshift_LG <- ifelse(enable_vshift, -0.05, 0)
vshift_Gw <- ifelse(enable_vshift,  0.05, 0)
vshift_Ro <- ifelse(enable_vshift, -0.10, 0)

c_NR <- c_HR <- c_N
c_UR <- 2*c_U + c_ex
# Li and Ghosh require lambda = (a+sqrt(a^2-4)) to be calculated, but this doesn't
# need to be recalculated every draw, hence it is a fixed cost component.
# Geweke doesn't calculate this at all (and so ER has no fixed cost component); I calculate
# it only when a is below the a0s threshold.
c_ER_fixed <- c_sqrt
# Drawing from a 1-sided exponential requires an exponential draw, a uniform draw, and
# an exponential calculation
c_ER1_draw <- c_Exp + c_U + c_ex
# Drawing from a 2-sided exponential is more complicated: we draw exponentials (and add a to them)
# until we get one less than b, THEN draw a uniform and calculate the exponential.  But the expected
# cost then depends on the likelihood of getting admissable Exp draws, i.e. it depends on the cdf
# of b-a, which also depends on the lambda parameter for the Exponential distribution being used.
Ec_ER2_draw <- function(b_less_a, lambda)
  c_Exp / pexp(b_less_a, lambda) + c_U + c_ex

graph_lim <- c(1,12.5)

# Enable this code to get all the ratios equal to 1, which should correspond to Li and Ghosh
if (F) {
  c_U <- 1
  c_NR <- 1
  c_HR <- 1
  c_UR <- 1
  c_ER_fixed <- 0
  c_ER1_draw <- 1
  c_ER2_draw <- function(...) 1
  c_ex <- 0
  c_sqrt <- 0
  a0 <- 0.2570
  graph_lim <- c(1,3)
}

# Ratios (relative to U[a,b] draw time)
cr_NR <- c_NR / c_U
cr_HR <- c_HR / c_U
cr_UR <- c_UR / c_U
cr_ER_fixed <- c_ER_fixed / c_U
cr_ER1_draw <- c_ER1_draw / c_U
cr_ER2_draw <- function(...) Ec_ER2_draw(...) / c_U

lambda <- function(a) (a+sqrt(a*a+4))/2

# Acceptance probabilities as a function of a and b:
p_NR <- function(a, b=Inf) if (any(a >= b)) stop("p_NR error: a >= b") else ifelse(a > 0, pnorm(a, low=F) - pnorm(b, low=F), pnorm(b) - pnorm(a))
p_HR <- function(a, b=Inf) if (any(a < 0)) stop("p_HR error: a < 0") else 2*p_NR(a, b)
p_ER_lam <- function(a, lam, b) sqrt(2*pi)*lam*exp(lam*a - lam^2/2)*p_NR(a,b)
p_ER <- function(a, b=Inf) p_ER_lam(a, lambda(a), b)

# Geweke also proposed an ER, but simplified the calculation by using lambda=a for
# the exponential (to avoid the sqrt); this lowers the acceptance rate, but also reduces
# computational complexity.  I also use this, but only above a pre-calculated threshold.
p_ER_s <- function(a, b=Inf) p_ER_lam(a, a, b)

plot(1, type="n", xlab=expression(a), ylab="Draw time (relative to uniform draw time)",
     xlim=c(-2, 5), ylim=graph_lim,
     main=expression("Case 1: " ~ b == infinity))

legend("topleft", legend=c("Proposed", "Li & Ghosh", "Geweke", "Robert"),
       col=c(col,col_LG,col_Gw,col_Ro), lty=c(lty,lty_LG,lty_Gw,lty_Ro), lwd=lwd
       )

# Adds piecewise functions to the current plot.
#
# funcs - a vector of functions
# jumps - a vector of x values where the function jumps; must be 1 shorter than funcs
# vshift - if non-zero (0 = default), a vertical adjustment to apply to the function (for visual distinguishing)
# ... - extra things to pass to curve (e.g. col and/or lty)
#
# The function[1] is drawn from -infinity to change[1], function[2] from change[1] to change[2],
# ..., function[n] from change[n-1] to +infinity.
curve_piecewise <- function(funcs, jumps, vshift=0, ...) {
  if (length(funcs) != 1 + length(jumps)) stop("curve_piecewise error: #funcs != 1+#jumps")
  for (i in 1:length(funcs)) {
    from <- if (i == 1) NULL else jumps[i-1]
    to <- if (i == length(funcs)) NULL else jumps[i]
    curve(funcs[[i]](x) + vshift, add=T, from=from, to=to, lwd=lwd, ...)
    if (i > 1)
      lines(c(jumps[i-1], jumps[i-1]), vshift + c(funcs[[i-1]](jumps[i-1]), funcs[[i]](jumps[i-1])), lwd=lwd, ...)
  }
}

# Ours:
# Normal rejection for a < 0; then half-normal until a0, then exponential:
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    function(a) cr_HR / p_HR(a),
    function(a) cr_ER_fixed + cr_ER1_draw / p_ER(a),
    function(a) cr_ER1_draw / p_ER_s(a)
  ),
  c(0, a0, a0s),
  col=col, lty=lty
)

# Li and Ghosh
# Normal for a < 0; then half-normal until a0_LG, then exponential:
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    function(a) cr_HR / p_HR(a),
    function(a) cr_ER_fixed + cr_ER1_draw / p_ER(a)
  ),
  c(0, a0_LG),
  vshift=vshift_LG,
  col=col_LG, lty=lty_LG
)

# Geweke:
# Normal for a <= a0_Gw; then simplified exponential:
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    function(a) cr_ER1_draw / p_ER_s(a)
  ),
  c(a0_Gw),
  vshift=vshift_Gw,
  col=col_Gw, lty=lty_Gw
)

# Robert:
# Normal for a <= 0, exponential after that:
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    # FIXME: does Robert require the sqrt?  Check it.
    function(a) cr_ER_fixed + cr_ER1_draw / p_ER(a)
  ),
  c(0),
  vshift=vshift_Ro,
  col=col_Ro, lty=lty_Ro
)

# Half-normal rejection for 0 <= a <= a0
#curve(cr_HR / p_HR(x), col="black", add=T, from=0, to=a0)
#curve(cr_HR / p_HR(x), col=rgb(0,0,1,0.5), add=T, from=a0, lty=2)


# Exponential rejection for a > a0
#curve(cr_ER / p_ER(x),
#      col=rgb(0,0.5,0,1), add=T, from=a0)
#curve(cr_ER / p_ER(x),
#      col=rgb(0,0.5,0,0.5), add=T, from=0, to=a0, lty=2)

# Add a line at Li and Ghosh's a0 and Geweke's a0 to show the inefficiency
#abline(v=a0_LG, lty=2)
#abline(v=a0_LG, lty=2)
