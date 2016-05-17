# Eris overview

Eris itself is designed for problems that can be divided into sequential
iterations supporting multiple types of interaction between stages.  The
simulation itself consists of a collection of simulation members, where each
member is an agent, a market, a good, or simply a generic object that doesn't
fit into one of these categories.

These objects then advance over time, with each period of time divided into
several conceptual stages during which agents learn, take actions, or perform
whatever various other tasks the eris-using application desired.

## Model stages

An iteration of the simulation is divided conceptually into several
"inter-period" and "intra-period" stages, where each simulation object may, if
desired, take some actions in each stage.  A single stage of the simulation
proceeds as follows:

- The simulation time period (t) is incremented.
- Inter-period optimization is performed.  This is designed to capture actions
  that agents perform while transitioning from one stage to the next, and
  consists of the following stages:
  - "Begin."  This phase is intended to perform various tasks related to the
    transition between phases.  For example, any unsold inventory produced in
    the previous period could spoil in this phase.
  - "Optimize."  This phase is intended for agents to choose their course of
    action for the upcoming period.  For example, a producer might decide on a
    production level.  Actions taken here should typically be private (i.e. not
    affecting other agents).
  - "Apply."  This phase exists to allow all agents to implement the action
    decided upon in the Optimize phase.  For example, a producer might incur
    production costs (for the quantity decided in the Optimize stage) in this
    phase.
  - "Advance."  This phase can be used for any final tasks that need to be
    before beginning the intra-period optimization phase.
- Next the intra-period optimization phase occurs, which is designed to capture
  actions taken within a period--for example, interacting with markets within
  the model.  This phase is broken down into the following stages:
  - "Initialize" - this can be used to perform any initial stage actions not
    done during the inter-period transition, for example, workers receive
    exogenous income.
  - "Reset" - this is called to reset any optimization decisions from the
    Optimization stage (see the following Reoptimize stage for details).
  - "Optimize" - in this stage, agents decide on their action based on the
    current simulation stage.  For example, consumers might decide how many
    units of a good to buy at the current market price.
  - "Reoptimize" - this stage exists to allow any simulation object to restart
    the optimization phase for all agents based on decisions made in the
    Optimize phase.  This can be enormously computationally expensive, of
    course, but is needed, for example, when using a Walrasian auctioneer that
    chooses a market price by experimenting with several potential prices until
    the market clears.  If any simulation object triggers a reoptimization, the
    intra-period optimization returns the to "Reset" phase.
  - "Apply" - this stage applies the choices made during the Optimize stage;
    the stage is only reached if no simulation member triggers a
    reoptimization.
  - "Finish" - this phase can take care of any final actions that need to be
    done at the end of a stage-- for example, receiving the proceeds from the
    period.  (This could also be done in the subsequent period's inter-period
    Begin phase.)

Any simulation object can register itself as an "optimizer" for one or more
stages by implementing the relevant optimization class, which essentially just
lets the main simulation loop know which methods need to be called on which
objects in each stage of the simulation.

The actions suggested for each stage are, of course, just guidelines: many
models won't make use of the stages suggested above.  They are also divisible:
a model can break up each stage into many substages if more precise stage
ordering is needed than fit in the structure above.

Each individual stage is completed for all simulation members before the
simulation proceeds to the following stage.  In practice, this essentially
allows a model to implement a simultaneous decision processes among agents,
even though the underlying computational implementation is not simultaneous:
all agents decide upon--but do not follow through with--an action in the
Optimize stage, and so each agent's action is not affected by the (current)
decision of other agents.  Only after all agents have made a decision does the
simulation advance to the Apply stage, where agents actually implement the
decided upon action.

## Built-in simulation members

Eris provides various optional, but useful, helper classes to facilitate
agent-based modelling such as a Bundle class to store bundles of goods; basic
agents which possess a bundle of assets; Market classes that simplify market
transactions; code to handle spatial positions (including wrapped, i.e.
toroidal, spaces); and a Bayesian linear implementation for representing
beliefs.  It also provides multithreading support and associated locking
interfaces (to prevent actions from colliding).

As a library, eris is not designed to provide an agent-based model itself, but
rather to provide a framework that takes care of the underlying pieces and
structure needed to build a sophisticated agent-based model.

