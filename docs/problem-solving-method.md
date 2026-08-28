# From Problem Statement to Correct System

You can design much of a solution before touching the computer. Paper, a diagram,
or a small table is often the best starting point. The goal is not to memorize the
whole statement. The goal is to turn prose into an explicit model that can later be
translated into code.

Understanding and implementation are still iterative: build a model, implement a
small slice, discover ambiguities, return to the statement, and refine the model.

## 1. Extract the rules

Read the statement section by section. For each section, answer:

- What entities exist?
- What persistent information belongs to each entity?
- What events can happen?
- What actions or commands may the solution produce?
- What operations are illegal?
- What consumes time or resources?
- What determines correctness?
- What determines the score?

Do not design classes yet. First write down facts using the language of the
problem.

For this scheduler, the important entities include requests, edge/cloud servers,
tasks, transfers, events, timing curves, and assignments.

## 2. Separate persistent state from temporary messages

For every piece of information, ask who owns it and how long it must exist.

- Persistent state survives between frames: request progress, server occupancy,
  selected remote, completed layers, and tokens produced.
- Events report that something happened: `ARR`, `TDN`, `XDN`, and `FIN`.
- Tasks describe temporary work assigned to a resource.
- Assignments are commands produced by the scheduler.

An event is not the request itself. A task is not the request itself. They may
refer to a request, while the request remains persistent in the world state.

## 3. Derive the state machine

List every state an entity can occupy. Then describe every legal transition.
For each transition, write:

```text
Trigger/event:
Preconditions:
State read:
State changed:
New state:
Resources acquired or released:
New work made ready:
```

Example:

```text
Trigger/event: P PRE task completes
Preconditions: the edge is running P PRE for request r
State changed: edge state and request r
New state: request r waits for its prefill upload
Resources released: edge becomes free
New work made ready: an UP transfer for request r
```

If a transition cannot be described precisely in words, it is not ready to be
implemented.

## 4. Write invariants

Invariants are facts that must always remain true. They provide the foundation for
assertions and tests.

Examples:

- A new `ARR` request ID equals the current number of stored requests.
- A `FIN` request ID refers to an existing request.
- A busy server has exactly one in-flight task.
- A server cannot execute two tasks simultaneously.
- A finished request can never return to a ready state.
- A request cannot be processed by two clouds simultaneously.
- Every assignment must be legal in the current world state.

For each transition, verify that its preconditions hold and that all invariants
still hold afterward.

## 5. Choose the smallest useful design

Only after the model is clear should the code structure be chosen. Prefer the
smallest design that represents the rules and makes illegal behavior difficult.

A useful separation for an event-driven scheduler is:

- Protocol parsing: text input into typed events.
- World state: persistent requests and resources.
- State transitions: applying events to the world.
- Scheduling policy: deciding which legal work to start.
- Protocol output: assignments into the required text format.
- Timing/scoring model: durations, costs, and optimization decisions.

Do not optimize for the most elegant possible architecture before an end-to-end
solution works. Refactor when a real limitation becomes visible.

## 6. Implement one vertical slice

Make one simple case travel through the entire system before generalizing:

```text
ARR
  -> P PRE
  -> prefill upload
  -> P PROC
  -> prefill download
  -> P POST
  -> decode stages
  -> FIN
```

Start with one request, one cloud, full prefill processing, and decode batches of
one. After that works, add concurrency, batching, placement policies, and scoring
optimizations.

## 7. Test the model, not only the syntax

Use three levels of tests:

1. Example tests: reproduce a known scenario from the statement.
2. Boundary tests: first and last layer, empty frames, simultaneous events, one
   server, and one request.
3. Invariant tests: deliberately try to cause illegal transitions or assignments.

A passing test only proves the tested behavior. Strong correctness comes from a
combination of precise transitions, explicit invariants, carefully selected tests,
and end-to-end execution.

## 8. Practice independent derivation

Before requesting help:

1. Write what you believe the rule means.
2. Write your proposed state transition.
3. State why it should be correct.
4. Implement a small attempt.
5. Write tests that could disprove your assumption.
6. Ask for review of your reasoning and identify any remaining uncertainty.

For example:

> I think `FIN` requires `rid < requests.size()` because it references an
> existing request, while `ARR` requires `rid == requests.size()` because it
> creates the next request.

This changes help from receiving the reasoning to having your own reasoning
challenged and improved.

## Reusable worksheet

Before coding a new system, fill this in:

```text
Goal:
Inputs:
Outputs:

Entities:
Persistent state owned by each entity:
Temporary messages/events:
Allowed actions:

States:
Legal transitions:
Illegal transitions:
Global invariants:

Simplest complete vertical slice:
Example tests:
Boundary tests:
Invariant tests:

Only after correctness works:
Performance bottlenecks:
Scoring/optimization opportunities:
```

The recurring engineering process is:

```text
prose -> entities -> ownership -> states -> transitions -> invariants -> tests -> code
```
