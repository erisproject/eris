rm(list=ls())

# Costs from draw-perf (for stl)
#c_N <- 39.7509; c_U <- 10.4649; c_Exp <- 40.6493; c_ex <- 17.3252; c_ex_T2 <- 0.3782; c_sqrt <- 4.5262; a0 <- 0.7252; a0s <- 2.6914; a1 <- 1.1384

# Costs from draw-perf (for boost)
c_N <- 11.6967; c_U <- 7.5142; c_Exp <- 33.2705; c_ex <- 17.3252; c_ex_T2 <- 0.3782; c_sqrt <- 4.5262; a0 <- 1.3728; a0s <- 2.4695; a1 <- 1.0248

# Costs from draw-perf (for gsl-zigg)
#c_N <- 11.2659; c_U <- 7.2304; c_Exp <- 24.3954; c_ex <- 17.3252; c_ex_T2 <- 0.3782; c_sqrt <- 4.5262; a0 <- 1.3076; a0s <- 2.2546; a1 <- 0.9260

# Costs from draw-perf (for gsl-rat)
#c_N <- 26.1169; c_U <- 7.2304; c_Exp <- 24.3954; c_ex <- 17.3252; c_ex_T2 <- 0.3782; c_sqrt <- 4.5262; a0 <- 0.7987; a0s <- 2.2546; a1 <- 0.9260

# Costs from draw-perf (for gsl-BoxM)
#c_N <- 54.3899; c_U <- 7.2304; c_Exp <- 24.3954; c_ex <- 17.3252; c_ex_T2 <- 0.3782; c_sqrt <- 4.5262; a0 <- 0.2263; a0s <- 2.2546; a1 <- 0.9260


# I could add timings for multiplications and additions, too, but when combined with other operations
# they often require no extra time.  For example, to compute:
#     0.5*(a + sqrt(a*a+4))
# appears (based on testing) to take exactly as long to compute as just sqrt(a), i.e. we
# are getting the other calculations essentially for free (at least on my Intel 2500K CPU),
# presumably because the CPU is able to perform the multiplications and additions using other
# execution units while the sqrt completes.  Even if this wasn't the case, however, they are
# extremely fast calculations relative to the above timings (around 19 times faster than c_sqrt).


c_NR <- c_HR <- c_N
c_UR <- 2*c_U + c_ex

# Drawing from a 1-sided exponential requires an exponential draw, a uniform draw, and
# an exponential calculation
c_ER1_draw <- c_Exp + c_U + c_ex
# Drawing from a 2-sided exponential is more complicated: we draw exponentials (and add a to them)
# until we get one less than b, THEN draw a uniform and calculate the exponential.  But the expected
# cost then depends on the likelihood of getting admissable Exp draws, i.e. it depends on the cdf
# of b-a, which also depends on the lambda parameter for the Exponential distribution being used.
Ec_ER2_draw <- function(b_less_a, lambda)
  c_Exp / pexp(b_less_a, lambda) + c_U + c_ex


# Enable this code to get all the ratios equal to 1, which should correspond to Li and Ghosh
if (F) {
  c_U <- 1
  c_NR <- 1
  c_HR <- 1
  c_UR <- 1
  c_ER1_draw <- 1
  c_ER2_draw <- function(...) 1
  c_ex <- 0
  c_ex_T2 <- 0
  c_sqrt <- 0
  a0 <- 0.2570
}

# Ratios (relative to U[a,b] draw time)
cr_NR <- c_NR / c_U
cr_HR <- c_HR / c_U
cr_UR <- c_UR / c_U
cr_ER1_draw <- c_ER1_draw / c_U
cr_ER2_draw <- function(...) Ec_ER2_draw(...) / c_U
cr_sqrt <- c_sqrt / c_U
cr_ex <- c_ex / c_U
cr_ex_T2 <- c_ex_T2 / c_U


# Functions we use (the constants are above)
b1_T2 <- function(a) a + c_HR / c_UR * sqrt(pi/2) * (
  # The optimal (ignoring costs) is exp(a^2/2) here (see b1_LG, below); use a 2nd-order
  # Taylor approximation instead.  The efficiency loss of doing so is less than the cost
  # of calculating the e^x.  There are further efficiency gains of a T3 or T4 approximation,
  # with different thresholds; but we just focus on T2 (at least for now).
  1 + a^2/2 + a^4/8 # T_2 approximation of e^x for x = a^2/2
)

# I'm calling this "full" but it isn't really complete: in particular, it performs the calculation
# as if the intermediate Exp draw is always admissable: but of course it isn't for two-sided
# truncation: when the draw is > b, we have to redraw.  The precise probability depends on both
# a and b, but is typically fairly high.
b2_full <- function(a) a + cr_ER1_draw/cr_UR * 2/(a + sqrt(a^2+4))*exp((a^2-a*sqrt(a^2+4))/4 + 1/2)
# For a > a1, it's more efficient to use 1/a instead of the messy calculation above; below
# a1, this can be less efficient.  What's a little bit interesting is that 1/a overestimates
# the true value, above (with this error descending to 0 as a increases), but in practice that
# ends up yielding a *better* threshold value, because the above underestimates the true value
# (by not taking into account the exected number of Exp draws required).
b2_simp <- function(a) a + cr_ER1_draw/cr_UR * 1/a

# Li and Ghosh constant/functions:
# a threshold for switching to exponential
a0_LG <- 0.2570
b1_LG <- function(a) a + sqrt(pi/2) * exp(a^2/2)
b2_LG <- function(a) a + 2/(a + sqrt(a^2+4))*exp((a^2-a*sqrt(a^2+4))/4 + 1/2)

# Geweke constants:
# phi(a), phi(b) thresholds for 0 \in [a,b] truncation: if either is less than this, prefer NR
t1_Gw <- 0.150
# phi(a)/phi(b) ratio below which we use UR
t2_Gw <- 2.18
# "a" value, when not using UR (according to t2_Gw), below which we use HR, above which, ER.
t3_Gw <- 0.725
# "a" threshold for switching to exponential (with simplified lambda), [a,inf) one-sided truncation
t4_Gw <- 0.45

# Translated Geweke constants:
# The a/b thresholds corresponding to t1_Gw:
b1_Gw <- sqrt(-2*log(sqrt(2*pi)*t1_Gw))
a1_Gw <- -b1_Gw
# Translate t2_Gw into a value of b (conditional on a):
b2_Gw <- function(a) sqrt(a^2 + 2*log(t2_Gw))

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

# When showing the same thing twice (e.g. for either-side-of-a-threshold cases), use this
# colour for the second one:
col_alt <- rgb(0.5,0,0.5,1)

# Shift the lines vertically by this amount for the older implementations to make them visible
# For precision, set this to F, for a less precise but easier to read graph, T
enable_vshift <- F
vshift_LG <- ifelse(enable_vshift, -0.05, 0)
vshift_Gw <- ifelse(enable_vshift,  0.05, 0)
vshift_Ro <- ifelse(enable_vshift, -0.10, 0)


lambda <- function(a) (a+sqrt(a*a+4))/2

# Acceptance probabilities as a function of a and b; returns NA if b <= a
p_NR <- function(a, b=Inf) ifelse(b <= a, NA, ifelse(b > -a, pnorm(a, low=F) - pnorm(b, low=F), pnorm(b) - pnorm(a)))

p_HR <- function(a, b=Inf) if (any(a < 0)) stop("p_HR error: a < 0") else 2*p_NR(a, b)

p_ER_lam <- function(a, lam, b) sqrt(2*pi)*lam*exp(lam*a - lam^2/2)*p_NR(a,b)
p_ER <- function(a, b=Inf) p_ER_lam(a, lambda(a), b)
# Geweke also proposed ER, but simplified the calculation by using lambda=a for
# the exponential (to avoid the sqrt); this lowers the acceptance rate, but also reduces
# computational complexity.  I also use this, but only above a pre-calculated threshold.
p_ER_s <- function(a, b=Inf) p_ER_lam(a, a, b)

p_UR <- function(a, b) sqrt(2*pi) / (b-a) * p_NR(a, b) * ifelse(a > 0, exp(a^2/2), 1)

# Adds piecewise functions to the current plot.
#
# funcs - a vector of functions
# jumps - a vector of x values where the function jumps; must be 1 shorter than funcs
# vshift - if non-zero (0 = default), a vertical adjustment to apply to the function (for visual distinguishing)
# ... - extra things to pass to curve (e.g. col and/or lty)
#
# funcs[1] is drawn from the current left xlim to change[1], funcs[2] from change[1] to change[2],
# ..., funcs[n] from change[n-1] to right xlim.  Functions with any change intervals outside the current
# graph x limits are skipped; for example, if the graph xlim is c(0,1) and change=c(-0.5) then funcs[1]
# is skipped and funcs[2] is plotted without any limits.
curve_piecewise <- function(funcs, jumps=c(), vshift=0, ...) {
  bounds <- par("usr")
  left_bound <- bounds[1]
  right_bound <- bounds[2]
  if (length(funcs) != 1 + length(jumps)) stop("curve_piecewise error: #funcs != 1+#jumps")
  for (i in 1:length(funcs)) {
    from <- ifelse(i == 1, left_bound, jumps[i-1])
    to <- ifelse(i == length(funcs), right_bound, jumps[i])
    if ((to <= left_bound) # upper bound is off the left
        ||
        (from >= right_bound) # Lower bound is off the right
    ) next
    if (from < left_bound) from <- left_bound
    if (to > right_bound) to <- right_bound

    curve(funcs[[i]](x) + vshift, add=T, from=from, to=to, lwd=lwd, ...)
    if (i > 1 && jumps[i-1] >= from && jumps[i-1] <= to)
      lines(c(jumps[i-1], jumps[i-1]), vshift + c(funcs[[i-1]](jumps[i-1]), funcs[[i]](jumps[i-1])), lwd=lwd, ...)
  }
}








# Case 1: a < 0 < b; a and b finite

# Produce a plot for various values of a
for (casea in c(-2, -1.4, -1, -0.5, -0.1)) {
  plot(1, type="n", xlab=expression(b), ylab="Draw time (relative to uniform draw time)",
       xlim=c(0, 5), ylim=c(1,12), xaxs="i", yaxs="i",
       main=bquote("Case 1: " ~ a == .(casea) ~ "; " ~ b > 0)
       )

  legend_items <- c("Proposed", "Robert; Li & Ghosh", "Robert (via Li & Ghosh)", "Geweke")
  legend_cols <- c(col,col_LG,col_Ro,col_Gw)
  legend_ltys <- c(lty,lty_LG,lty_Ro,lty_Gw)
  if (casea == -1.4) {
    # Geweke threshold case
    legend_items[length(legend_items)+1] <- "Geweke (a=-1.398)"
    legend_cols[length(legend_cols)+1] <- col_alt
    legend_ltys[length(legend_ltys)+1] <- lty_Gw
  }
  legend("topleft", legend=legend_items, col=legend_cols, lty=legend_ltys, lwd=lwd)

  # Ours:
  # Uniform rejection for b < a + sqrt(2pi)*c_NR/c_UR, Normal rejection after that
  curve_piecewise(
    c(function(b) cr_UR / p_UR(casea,b),
      function(b) cr_NR / p_NR(casea,b)
    ),
    c(casea + sqrt(2*pi)*c_NR/c_UR), col=col, lty=lty
  )

  # Robert: Uniform rejection for b - a < sqrt(2pi).
  # Li and Ghosh is identical, but claim Robert's result as their own, apparently
  # missing the part where Robert says this is more efficientonly for b - a < sqrt(2pi).
  curve_piecewise(
    c(function(b) cr_UR / p_UR(casea,b),
      function(b) cr_NR / p_NR(casea,b)
    ),
    c(casea + sqrt(2*pi)),
    col=col_LG, lty=lty_LG, vshift=vshift_LG
  )

  # Robert, as claimed by Li and Ghosh:
  curve_piecewise(
    c(function(b) cr_UR / p_UR(casea,b)),
    col=col_Ro, lty=lty_Ro, vshift=vshift_Ro
  )

  # Geweke:
  # Normal if phi(a) <= t1_Gw or phi(b) <= t1_Gw
  # Or, equivalently, a <= a1_Gw or b >= b1_Gw
  curve_piecewise(
    c(function(b) cr_UR / p_UR(casea,b),
      function(b) cr_NR / p_NR(casea,b)
    ),
    c(ifelse(casea <= a1_Gw, -1e10, b1_Gw)),
    col=col_Gw, lty=lty_Gw, vshift=vshift_Gw
  )

  # For the Geweke near-threshold case (-1.4), add a plot for -1.398, which is on
  # the other side of the threshold from -1.4
  if (casea == -1.4)
    curve_piecewise(
      c(function(b) cr_UR / p_UR(-1.398,b),
        function(b) cr_NR / p_NR(-1.398,b)
      ),
      c(b1_Gw),
      vshift=vshift_Gw, col=col_alt, lty=lty_Gw
    )
}












# Case 2, in the special case 0 <= a, b = infinity
# We plot a values for this (fixed) b

plot(1, type="n", xlab=expression(a), ylab="Draw time (relative to uniform draw time)",
     xlim=c(-2.5, 5.5), ylim=c(1,13), xaxs="i", yaxs="i",
     main=expression("Case 1: " ~ b == infinity))

legend("topleft", legend=c("Proposed", "Li & Ghosh", "Geweke", "Robert"),
       col=c(col,col_LG,col_Gw,col_Ro), lty=c(lty,lty_LG,lty_Gw,lty_Ro), lwd=lwd
       )


# Ours:
# Normal rejection for a < 0; then half-normal until a0, then exponential until a0s, then
# simplified exponential
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    function(a) cr_HR / p_HR(a),
    function(a) cr_sqrt + cr_ER1_draw / p_ER(a),
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
    function(a) cr_sqrt + cr_ER1_draw / p_ER(a)
  ),
  c(0, a0_LG),
  vshift=vshift_LG, col=col_LG, lty=lty_LG
)

# Geweke:
# Normal for a <= t4_Gw; then simplified exponential:
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    function(a) cr_ER1_draw / p_ER_s(a)
  ),
  c(t4_Gw),
  vshift=vshift_Gw, col=col_Gw, lty=lty_Gw
)

# Robert:
# Normal for a <= 0, exponential after that:
curve_piecewise(
  c(
    function(a) cr_NR / p_NR(a),
    # FIXME: does Robert require the sqrt?  Check it.
    function(a) cr_sqrt + cr_ER1_draw / p_ER(a)
  ),
  c(0),
  vshift=vshift_Ro, col=col_Ro, lty=lty_Ro
)




# Case 2, 0 <= a < b < infinity; plot b for various a
for (casea in c(0, a0_LG, 0.5, t3_Gw, 1, 1.1, 1.25, 1.5, 2, 5, 10)) {
  plot(1, type="n", xlab=expression(b-a), ylab="Draw time (relative to uniform draw time)",
       xlim=c(0, 5), # These are b-a values
       ylim=c(0,25),
       xaxs="i", yaxs="i",
       main=bquote("Case 2: " ~ a == .(casea)))

  if (casea < a0_LG)
    legend("topright", legend=c("Proposed", "Li & Ghosh", "Robert", "Geweke"),
           col=c(col,col_LG,col_Ro,col_Gw), lty=c(lty,lty_LG,lty_Ro,lty_Gw), lwd=lwd)
  else if (casea == a0_LG)
    legend("topright", legend=c("Proposed", "Robert; Li & Ghosh", "Li & Ghosh (a=0.256)", "Geweke"),
           col=c(col,col_Ro,col_LG,col_Gw), lty=c(lty,lty_Ro,lty_LG,lty_Gw), lwd=lwd)
  else if (casea == t3_Gw)
    legend("topright", legend=c("Proposed", "Robert; Li & Ghosh", "Geweke (a=0.725)", "Geweke (a=0.724)"),
           col=c(col,col_Ro,col_Gw,col_alt), lty=c(lty,lty_Ro,lty_Gw,lty_Gw), lwd=lwd)
  else
    legend("topright", legend=c("Proposed", "Robert; Li & Ghosh", "Geweke"),
           col=c(col,col_Ro,col_Gw), lty=c(lty,lty_Ro,lty_Gw), lwd=lwd)

  # Precalculate this (used in mine, L&G, and Robert):
  lam_a <- lambda(casea)

  # Ours:
  # If a < a0:
  #   if b < b1_T2(a): UR
  #   else: HR
  # else:
  #   if a < a1 and b < b2(a): UR
  #   if a >= a1 and b < b2_s(a): UR
  #   else: ER
  # But note that a < a1 may be trivally satisfied by the computation costs, in which case
  # it doesn't even need to be evaluated (i.e. it can be eliminated at compile time).
  if (casea <= a0) {
    myfuncs <- c(
      function(bma) cr_ex_T2 + cr_UR / p_UR(casea,bma+casea),
      function(bma) cr_ex_T2 + cr_HR / p_HR(casea,bma+casea)
    )
    mychange <- c(b1_T2(casea)-casea)
  }
  else if (casea >= a1) {
    myfuncs <- c(
      function(bma) cr_UR / p_UR(casea, bma+casea),
      # FIXME: I think I can do better than this
      function(bma) cr_sqrt + cr_ER2_draw(bma, lam_a) / p_ER(casea, bma+casea)
    )
    mychange <- c(b2_simp(casea)-casea)
  }
  else {
    myfuncs <- c(
      function(bma) cr_ex + cr_sqrt + cr_UR / p_UR(casea, bma+casea),
      function(bma) cr_ex + cr_sqrt + cr_ER2_draw(bma, lam_a) / p_ER(casea, bma+casea)
    )
    mychange <- c(b2_full(casea)-casea)
  }
  curve_piecewise(myfuncs, mychange, col=col, lty=lty)

  # Li & Ghosh:
  # a < a0:
  #   b <= b1(a): UR
  #   else: HR
  # else:
  #   b <= b2(a): UR
  #   else: ER

  if (casea < a0_LG) {
    curve_piecewise(
      c(
        function(bma) cr_ex + cr_UR / p_UR(casea, bma+casea),
        function(bma) cr_ex + cr_HR / p_HR(casea, bma+casea)
      ),
      c(b1_LG(casea)-casea),
      vshift=vshift_LG, col=col_LG, lty=lty_LG
    )
  }
  # For the threshold case (a0_LG), add a plot for a=0.256
  else if (casea == a0_LG) {
    curve_piecewise(
      c(
        function(bma) cr_ex + cr_UR / p_UR(0.256, bma+casea),
        function(bma) cr_ex + cr_HR / p_HR(0.256, bma+casea)
      ),
      c(sqrt(pi/2)*exp(0.256^2/2)),
      vshift=vshift_LG, col=col_LG, lty=lty_LG
    )
  }
  # Otherwise, for a > a0_LG, this is just Robert.

  # Robert: Identical to Li and Ghosh a >= a0 case (but for all a >= 0), i.e. always
  # use either UR (for b <= b2(a)) or ER (b > b2(a)).  We also always need to calculate
  # one e^x and one sqrt just to decide which to do (but can save the sqrt for the ER)
  curve_piecewise(
    c(
      function(bma) cr_sqrt + cr_ex + cr_UR / p_UR(casea, bma+casea),
      function(bma) cr_sqrt + cr_ex + cr_ER2_draw(bma, lam_a) / p_ER(casea, bma+casea)
    ),
    c(b2_LG(casea) - casea),
    vshift=vshift_Ro, col=col_Ro, lty=lty_Ro
  )


  # Geweke: if phi(a)/phi(b) <= t2_Gw: UR
  # else:
  #   if a < t3_Gw: HR
  #   else: ER (simplified)
  #
  # The first condition simplifies to exp((b^2-a^2)/2) <= t2, which rearranges to:
  # b <= sqrt(a^2 + 2 ln(t2)).  Since the ln(t2) is a constant, it can be computed
  # at compilation time, so evaluating this requires a sqrt (but not an exp).
  if (casea < t3_Gw)
    rightfunc <- function(bma) cr_sqrt + cr_HR / p_HR(casea, bma+casea)
  else
    rightfunc <- function(bma) cr_sqrt + cr_ER2_draw(bma,casea) / p_ER_s(casea, bma+casea)

  curve_piecewise(
    c(
      function(bma) cr_sqrt + cr_UR / p_UR(casea, bma+casea),
      rightfunc
    ),
    c(b2_Gw(casea)-casea),
    vshift=vshift_Gw, col=col_Gw, lty=lty_Gw
  )

  # For the threshold case (t3_Gw), add a plot for a=0.724
  if (casea == t3_Gw) {
    curve_piecewise(
      c(
        function(bma) cr_sqrt + cr_UR / p_UR(casea, bma+casea),
        function(bma) cr_sqrt + cr_HR / p_HR(0.724, bma+casea)
      ),
      c(b2_Gw(0.724)-casea),
      vshift=vshift_Gw, col=col_alt, lty=lty_Gw
    )
  }
}

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
