rm(list=ls())

# Costs and thresholds calculated by draw-perf:
costs <- list(
  stl=list(N=39.7998, U=10.5046, Exp=40.5734, e.x=17.3242, e.x_T2=0.3778, sqrt=4.5276, a0=0.7240, a0s=2.6902, a1=1.1383),
  boost=list(N=11.9809, U=7.5338, Exp=33.3370, e.x=17.3242, e.x_T2=0.3778, sqrt=4.5276, a0=1.3609, a0s=2.4710, a1=1.0258),
  gsl.zigg=list(N=11.5016, U=6.9827, Exp=24.3539, e.x=17.3242, e.x_T2=0.3778, sqrt=4.5276, a0=1.2934, a0s=2.2471, a1=0.9215),
  gsl.ratio=list(N=26.0464, U=6.9827, Exp=24.3539, e.x=17.3242, e.x_T2=0.3778, sqrt=4.5276, a0=0.7968, a0s=2.2471, a1=0.9215),
  gsl.BoxM=list(N=54.3270, U=6.9827, Exp=24.3539, e.x=17.3242, e.x_T2=0.3778, sqrt=4.5276, a0=0.2224, a0s=2.2471, a1=0.9215),
  fairytale=list(N=1.0000, U=1.0000, Exp=1.0000, e.x=0.0000, e.x_T2=0.0000, sqrt=0.0000, a0=0.2570, a0s=Inf, a1=Inf)
);

# I could add timings for multiplications and additions, too, but when combined with other operations
# they often require no extra time.  For example, to compute:
#     0.5*(a + sqrt(a*a+4))
# appears (based on testing) to take exactly as long to compute as just sqrt(a), i.e. we
# are getting the other calculations essentially for free (at least on my Intel 2500K CPU),
# presumably because the CPU is able to perform the multiplications and additions using other
# execution units while the sqrt completes.  Even if this wasn't the case, however, they are
# extremely fast calculations relative to the above timings (around 19 times faster than c_sqrt).


for (lib in names(costs)) for (graph_inverse in (if (lib == "fairytale") c(T,F) else F)) {

c <- costs[[lib]]

# If true, draw the inverse graphs (that is, acceptance probabilities divided by costs).
# In particular, this enabled with the fairytale lib should reproduce the diagrams of
# Li and Ghosh.  This is fairly useless *except* in that case, where the costs are all 1,
# and so it actually depicts acceptance rates (but even then, it is misleading, which is
# the whole point).
#graph_inverse <- F



pdf_filename <- paste(sep="", "acceptance-", ifelse(graph_inverse, "rate-", "speed-"), lib, ".pdf")
cairo_pdf(pdf_filename, width=6, height=6, onefile=T)





c$NR <- c$HR <- c$N
c$UR <- 2*c$U + c$e.x

# Drawing from a 1-sided exponential requires an exponential draw, a uniform draw, and
# an exponential calculation
c$ER1_draw <- c$Exp + c$U + c$e.x
# Drawing from a 2-sided exponential is more complicated: we draw exponentials (and add a to them)
# until we get one less than b, THEN draw a uniform and calculate the exponential.  But the expected
# cost then depends on the likelihood of getting admissable Exp draws, i.e. it depends on the cdf
# of b-a, which also depends on the lambda parameter for the Exponential distribution being used.
c$E_ER2_draw <- function(b_less_a, lambda)
  c$Exp / pexp(b_less_a, lambda) + c$U + c$e.x

# In fairytale land, we pretend that the uniform draws for rejection sampling are is free,
# and that the Exp draw always gives us an admissable value (in the ER2 case)
if (lib == "fairytale") {
  c$UR = c$U + c$e.x
  c$ER1_draw <- c$Exp + c$e.x
  c$E_ER2_draw <- function(b_less_a, lambda) c$ER1_draw
}

# Ratios (relative to U[a,b] draw time)
cr <- list(
  NR=c$NR/c$U,
  HR=c$HR/c$U,
  UR=c$UR/c$U,
  ER1_draw=c$ER1_draw/c$U,
  ER2_draw=function(...) c$E_ER2_draw(...) / c$U,
  sqrt=c$sqrt/c$U,
  e.x=c$e.x/c$U,
  e.x_T2=c$e.x_T2/c$U
)


# Functions and constants we use:
a0 <- c$a0
a0s <- c$a0s
a1 <- c$a1
b1_T2 <- function(a) a + c$HR / c$UR * sqrt(pi/2) * (
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
b2_full <- function(a) a + cr$ER1_draw/cr$UR * 2/(a + sqrt(a^2+4))*exp((a^2-a*sqrt(a^2+4))/4 + 1/2)
# For a > a1, it's more efficient to use 1/a instead of the messy calculation above; below
# a1, this can be less efficient.  What's a little bit interesting is that 1/a overestimates
# the true value, above (with this error descending to 0 as a increases), but in practice that
# ends up yielding a *better* threshold value, because the above underestimates the true value
# (by not taking into account the exected number of Exp draws required).
b2_simp <- function(a) a + cr$ER1_draw/cr$UR * 1/a

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
enable_vshift <- T
vshift_LG <- ifelse(enable_vshift, -1*lwd, 0)
vshift_Gw <- ifelse(enable_vshift,  1*lwd, 0)
vshift_Ro <- ifelse(enable_vshift, -2*lwd, 0)

# Some hand-tuned scales (xbegin, xend, ybegin, yend)
graph_lim <- list(
  case1=c(0,5,0,12),
  case2=c(-2,5.5,0,13),
  case3=c(0,5,0,25)
)
if (lib == "fairytale") {
  graph_lim$case1[3:4] <- c(0.8,5)
  graph_lim$case2[3:4] <- c(0.8,3.2)
  graph_lim$case3 <- c(0,3,0.8,3.2)
}

graph_ylab <- "Average draw time (relative to U[0,1])"

# If inverting, change the vertical range to [0, 1.2]
if (graph_inverse) {
  for (case in names(graph_lim)) {
    graph_lim[[case]][3:4] <- c(0, 1.2)
  }
  
  if (lib == "fairytale") graph_ylab <- "Acceptance rate"
  else graph_ylab <- "Acceptance rate ÷ average draw time"
}




# The "optimal" lambda (ignoring the cost of the sqrt)
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
# vshift - if non-zero (0 = default), a vertical adjustment to apply to the function
#          (for visual distinguishing), in display units
# ... - extra things to pass to curve (e.g. col and/or lty)
#
# funcs[1] is drawn from the current left xlim to change[1], funcs[2] from change[1] to change[2],
# ..., funcs[n] from change[n-1] to right xlim.  Functions with any change intervals outside the current
# graph x limits are skipped; for example, if the graph xlim is c(0,1) and change=c(-0.5) then funcs[1]
# is skipped and funcs[2] is plotted without any limits.
#
# Note that, when graph_inverse is enabled, the plotted values are actually the inverse of the
# function values.
curve_piecewise <- function(funcs, jumps=c(), vshift=0, ...) {
  bounds <- par("usr")
  vshift_ <- grconvertY(c(0,vshift), from="device", to="user") 
  vshift_gr <- vshift_[2]-vshift_[1]
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

    maybe_inverse <- if (graph_inverse) function(v) 1/v else function(v) v

    curve(maybe_inverse(funcs[[i]](x)) + vshift_gr, add=T, from=from, to=to, lwd=lwd, ...)

    if (i > 1 && jumps[i-1] >= from && jumps[i-1] <= to)
      lines(
        c(jumps[i-1], jumps[i-1]),
        vshift_gr + maybe_inverse(c(funcs[[i-1]](jumps[i-1]), funcs[[i]](jumps[i-1]))),
        lwd=lwd,
        ...)
  }
}








# Case 1: a < 0 < b; a and b finite

# Produce a plot for various values of a
for (casea in c(-2, -1.4, -1, -0.5, -0.1)) {
  plot(1, type="n", xlab=expression(b), ylab=graph_ylab,
       xlim=graph_lim$case1[1:2], ylim=graph_lim$case1[3:4], xaxs="i", yaxs="i",
       main=paste(sep="", "Case 1: a = ", casea, "; b > 0\n(", lib, ")")
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
  legend(ifelse(graph_inverse, "bottomright", "topleft"), legend=legend_items, col=legend_cols, lty=legend_ltys, lwd=lwd)

  # Ours:
  # Uniform rejection for b < a + sqrt(2pi)*c_NR/c_UR, Normal rejection after that
  curve_piecewise(
    c(function(b) cr$UR / p_UR(casea,b),
      function(b) cr$NR / p_NR(casea,b)
    ),
    c(casea + sqrt(2*pi)*c$NR/c$UR), col=col, lty=lty
  )

  # Robert: Uniform rejection for b - a < sqrt(2pi).
  # Li and Ghosh is identical, but claim Robert's result as their own, apparently
  # missing the part where Robert says this is more efficientonly for b - a < sqrt(2pi).
  curve_piecewise(
    c(function(b) cr$UR / p_UR(casea,b),
      function(b) cr$NR / p_NR(casea,b)
    ),
    c(casea + sqrt(2*pi)),
    col=col_LG, lty=lty_LG, vshift=vshift_LG
  )

  # Robert, as claimed by Li and Ghosh:
  curve_piecewise(
    c(function(b) cr$UR / p_UR(casea,b)),
    col=col_Ro, lty=lty_Ro, vshift=vshift_Ro
  )

  # Geweke:
  # Normal if phi(a) <= t1_Gw or phi(b) <= t1_Gw
  # Or, equivalently, a <= a1_Gw or b >= b1_Gw
  curve_piecewise(
    c(function(b) cr$UR / p_UR(casea,b),
      function(b) cr$NR / p_NR(casea,b)
    ),
    c(ifelse(casea <= a1_Gw, -1e10, b1_Gw)),
    col=col_Gw, lty=lty_Gw, vshift=vshift_Gw
  )

  # For the Geweke near-threshold case (-1.4), add a plot for -1.398, which is on
  # the other side of the threshold from -1.4
  if (casea == -1.4)
    curve_piecewise(
      c(function(b) cr$UR / p_UR(-1.398,b),
        function(b) cr$NR / p_NR(-1.398,b)
      ),
      c(b1_Gw),
      vshift=vshift_Gw, col=col_alt, lty=lty_Gw
    )
}












# Case 2, in the special case 0 <= a, b = infinity
# We plot a values for this (fixed) b

plot(1, type="n", xlab=expression(a), ylab=graph_ylab,
     xlim=graph_lim$case2[1:2], ylim=graph_lim$case2[3:4], xaxs="i", yaxs="i",
     main=paste(sep="", "Case 2: b = ∞\n(", lib, ")"))

legend(ifelse(graph_inverse, "bottomright", "topleft"), legend=c("Proposed", "Li & Ghosh", "Geweke", "Robert"),
       col=c(col,col_LG,col_Gw,col_Ro), lty=c(lty,lty_LG,lty_Gw,lty_Ro), lwd=lwd
       )


# Ours:
# Normal rejection for a < 0; then half-normal until a0, then exponential until a0s, then
# simplified exponential
curve_piecewise(
  c(
    function(a) cr$NR / p_NR(a),
    function(a) cr$HR / p_HR(a),
    function(a) cr$sqrt + cr$ER1_draw / p_ER(a),
    function(a) cr$ER1_draw / p_ER_s(a)
  ),
  c(0, a0, a0s),
  col=col, lty=lty
)

# Li and Ghosh
# Normal for a < 0; then half-normal until a0_LG, then exponential:
curve_piecewise(
  c(
    function(a) cr$NR / p_NR(a),
    function(a) cr$HR / p_HR(a),
    function(a) cr$sqrt + cr$ER1_draw / p_ER(a)
  ),
  c(0, a0_LG),
  vshift=vshift_LG, col=col_LG, lty=lty_LG
)

# Geweke:
# Normal for a <= t4_Gw; then simplified exponential:
curve_piecewise(
  c(
    function(a) cr$NR / p_NR(a),
    function(a) cr$ER1_draw / p_ER_s(a)
  ),
  c(t4_Gw),
  vshift=vshift_Gw, col=col_Gw, lty=lty_Gw
)

# Robert:
# Normal for a <= 0, exponential after that:
curve_piecewise(
  c(
    function(a) cr$NR / p_NR(a),
    # FIXME: does Robert require the sqrt?  Check it.
    function(a) cr$sqrt + cr$ER1_draw / p_ER(a)
  ),
  c(0),
  vshift=vshift_Ro, col=col_Ro, lty=lty_Ro
)




# Case 3, 0 <= a < b < infinity; plot b for various a
for (casea in c(0, a0_LG, 0.5, t3_Gw, 1, 1.1, 1.25, 1.5, 2, 5, 10)) {
  plot(1, type="n", xlab=expression(b-a), ylab=graph_ylab,
       xlim=graph_lim$case3[1:2], # These are b-a values
       ylim=graph_lim$case3[3:4],
       xaxs="i", yaxs="i",
       main=paste(sep="", "Case 3: a = ", casea, ", b > a\n(", lib, ")"))

  legendpos <- ifelse(graph_inverse, "bottomright", "topright")
  if (casea < a0_LG)
    legend(legendpos, legend=c("Proposed", "Li & Ghosh", "Robert", "Geweke"),
           col=c(col,col_LG,col_Ro,col_Gw), lty=c(lty,lty_LG,lty_Ro,lty_Gw), lwd=lwd)
  else if (casea == a0_LG)
    legend(legendpos, legend=c("Proposed", "Robert; Li & Ghosh", "Li & Ghosh (a=0.256)", "Geweke"),
           col=c(col,col_Ro,col_LG,col_Gw), lty=c(lty,lty_Ro,lty_LG,lty_Gw), lwd=lwd)
  else if (casea == t3_Gw)
    legend(legendpos, legend=c("Proposed", "Robert; Li & Ghosh", "Geweke (a=0.725)", "Geweke (a=0.724)"),
           col=c(col,col_Ro,col_Gw,col_alt), lty=c(lty,lty_Ro,lty_Gw,lty_Gw), lwd=lwd)
  else
    legend(legendpos, legend=c("Proposed", "Robert; Li & Ghosh", "Geweke"),
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
      function(bma) cr$e.x_T2 + cr$UR / p_UR(casea,bma+casea),
      function(bma) cr$e.x_T2 + cr$HR / p_HR(casea,bma+casea)
    )
    mychange <- c(b1_T2(casea)-casea)
  }
  else if (casea >= a1) {
    myfuncs <- c(
      function(bma) cr$UR / p_UR(casea, bma+casea),
      # FIXME: I think I can do better than this
      function(bma) cr$sqrt + cr$ER2_draw(bma, lam_a) / p_ER(casea, bma+casea)
    )
    mychange <- c(b2_simp(casea)-casea)
  }
  else {
    myfuncs <- c(
      function(bma) cr$e.x + cr$sqrt + cr$UR / p_UR(casea, bma+casea),
      function(bma) cr$e.x + cr$sqrt + cr$ER2_draw(bma, lam_a) / p_ER(casea, bma+casea)
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
        function(bma) cr$e.x + cr$UR / p_UR(casea, bma+casea),
        function(bma) cr$e.x + cr$HR / p_HR(casea, bma+casea)
      ),
      c(b1_LG(casea)-casea),
      vshift=vshift_LG, col=col_LG, lty=lty_LG
    )
  }
  # For the threshold case (a0_LG), add a plot for a=0.256
  else if (casea == a0_LG) {
    curve_piecewise(
      c(
        function(bma) cr$e.x + cr$UR / p_UR(0.256, bma+casea),
        function(bma) cr$e.x + cr$HR / p_HR(0.256, bma+casea)
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
      function(bma) cr$sqrt + cr$e.x + cr$UR / p_UR(casea, bma+casea),
      function(bma) cr$sqrt + cr$e.x + cr$ER2_draw(bma, lam_a) / p_ER(casea, bma+casea)
    ),
    c(b2_LG(casea) - casea),
    vshift=ifelse(casea > a0_LG, vshift_LG, vshift_Ro), col=col_Ro, lty=lty_Ro
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
    rightfunc <- function(bma) cr$sqrt + cr$HR / p_HR(casea, bma+casea)
  else
    rightfunc <- function(bma) cr$sqrt + cr$ER2_draw(bma,casea) / p_ER_s(casea, bma+casea)

  curve_piecewise(
    c(
      function(bma) cr$sqrt + cr$UR / p_UR(casea, bma+casea),
      rightfunc
    ),
    c(b2_Gw(casea)-casea),
    vshift=vshift_Gw, col=col_Gw, lty=lty_Gw
  )

  # For the threshold case (t3_Gw), add a plot for a=0.724
  if (casea == t3_Gw) {
    curve_piecewise(
      c(
        function(bma) cr$sqrt + cr$UR / p_UR(casea, bma+casea),
        function(bma) cr$sqrt + cr$HR / p_HR(0.724, bma+casea)
      ),
      c(b2_Gw(0.724)-casea),
      vshift=vshift_Gw, col=col_alt, lty=lty_Gw
    )
  }
}

# Half-normal rejection for 0 <= a <= a0
#curve(cr$HR / p_HR(x), col="black", add=T, from=0, to=a0)
#curve(cr$HR / p_HR(x), col=rgb(0,0,1,0.5), add=T, from=a0, lty=2)


# Exponential rejection for a > a0
#curve(cr$ER / p_ER(x),
#      col=rgb(0,0.5,0,1), add=T, from=a0)
#curve(cr$ER / p_ER(x),
#      col=rgb(0,0.5,0,0.5), add=T, from=0, to=a0, lty=2)

# Add a line at Li and Ghosh's a0 and Geweke's a0 to show the inefficiency
#abline(v=a0_LG, lty=2)
#abline(v=a0_LG, lty=2)

dev.off()

}