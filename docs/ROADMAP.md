# Roadmap

This roadmap describes direction, not dates. Current support is whatever the tested source actually implements.

## Current foundation

- maintain runner connectivity/busy state, active-job discovery, history/export and local service inspection
- keep the Python application and native bridge behaviour aligned during migration
- use Common for shared theme and durable-file primitives

## Near-term priorities

- move more critical state interpretation and platform mechanics into first-party native code where it improves reliability
- reduce incidental dependence on command-output shape
- strengthen cross-platform parity and failure handling

## Longer-term direction

- converge on a predominantly native implementation while preserving user-visible behaviour and saved data
- add monitoring capability only where provider semantics can be represented clearly

## Admission rule

New work needs clear ownership and a realistic validation path. Research or inspiration is not itself an implementation commitment.

## Completion rule

An item is complete only when implementation, regression evidence, user-visible behaviour and maintained documentation agree.
