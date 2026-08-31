# Release pipeline assessment

Recommendation: **Worth exploring after runtime safety work**
Category: in-process plus local-substitutable

## Candidate A — deepen validated release policy

### Problem

`validate_profile()` performs substantial validation but returns a raw dictionary (`firmware/tools/release.py:116-185`). Profile schema knowledge then leaks into artifact discovery, ESP32 offsets, build metadata, receipt verification, packaging, and development Flash planning (`release.py:262-391`, `563-619`, `695-919`, `1224-1344`).

The shell entrypoint also parses release arguments before the Python parser interprets the translated command again (`firmware/tools/fw:343-505`; `release.py:1389-1431`).

### Deepening direction

Concentrate validated policy, resolved paths, offsets, and artifact selection in one deep release-policy module. Preserve canonical-path, symlink, and root-escape checks.

### Deletion test

Deleting `validate_profile()` would force its checks into every command, confirming useful depth already exists. The opportunity is to increase that depth so callers no longer understand raw schema details.

### Test surface

Current scoped tests cover toolchain metadata and development Flash planning. They do not cover schema evolution or the package flow.

## Candidate B — deepen trusted release manifests

### Problem

Packaging assembles a raw manifest inline (`release.py:873-921`). `verify_release()` independently interprets it (`release.py:946-1113`), then `plan_flash()` receives the returned dictionary and knows its exact shape (`release.py:1348-1386`).

### Deepening direction

Concentrate manifest construction, offline verification, and trusted queries in one deep manifest module. Package-to-verify-to-plan round trips and tamper cases become the interface test surface.

### Constraint

Offline verification must remain independent of the checkout's mutable profile. Deepening must not couple package verification back to current source configuration.

## Why this is not first

The existing release module already hides substantial security-sensitive implementation and its 11 focused tests pass. Runtime composition and telemetry identity have more immediate safety leverage.
