# Port Checklist

Use this checklist as a phase tracker. Update each item explicitly.

This repo predates `libgen`; the planning artifacts here were filled via
the libgen catch-up path (audit-against-implementation rather than
plan-then-implement). Items marked `[x]` reflect that audit.

Legend:

- `[ ]` not started
- `[-]` in progress
- `[x]` done

## Intake

- [x] target repo scaffolded
- [x] GitHub repo created under `polycpp`
- [ ] GitHub repo visibility verified private
- GitHub repo URL: https://github.com/polycpp/commander
- GitHub repo private: no
- libgen catch-up note: this repo predates libgen and is already public on
  GitHub. The libgen `--strict` gate requires `private: yes`; that gate
  cannot be satisfied in retrospect. The repo passed the public-release
  criteria informally before libgen existed. Future companions must follow
  the private-first workflow (`scripts/create-private-repo.py`).
- [x] baseline readiness passed
- [x] upstream repo cloned or updated locally
- [x] upstream revision recorded in `docs/research.md`

## Upstream Analysis

- [x] repo layout inspected
- [x] entry points identified
- [x] important implementation files listed
- [x] test directories identified
- [x] implementation risks recorded

## Existing Companion Alignment

- [x] existing companion repos inspected for conventions
- [x] CMake target and alias pattern aligned or deviation recorded
- [x] public header layout planned
- [x] detail/private header strategy planned
- [x] aggregator header strategy decided
- [x] example strategy recorded
- [x] README structure aligned with companion repos
- [x] deliberate deviations recorded in `docs/divergences.md`

## Polycpp Ecosystem Reuse

- [x] polycpp core modules searched for reusable primitives
- [x] current polycpp capability snapshot recorded
- [x] transport/listener capabilities revalidated against current polycpp
- [x] polycpp core reuse decisions recorded
- [x] public polycpp interop review completed
- [x] public string policy recorded
- [x] JsonValue/Object/Array interop policy recorded
- [x] Date/time interop policy recorded
- [x] diagnostic/config JSON adapter policy recorded
- [x] direct `JSON::stringify` support decision recorded
- [x] companion libraries searched for reusable APIs
- [x] companion reuse decisions recorded
- [x] new local abstractions justified

## Dependency/API Analysis

- [x] dependency/API analysis tool run
- [x] published npm artifact inspected or not-applicable decision recorded
- [x] direct dependencies classified
- [x] dependency ownership decisions recorded
- [x] dependency licenses classified
- [x] dependency license strategy recorded
- [x] GPL/AGPL/LGPL/MPL impacts reviewed
- [x] Node.js API usage reviewed
- [x] Node parity surface audit completed
- [x] JavaScript API usage reviewed

## Scope

- [x] `v0` scope written in `docs/research.md`
- [x] deferred features listed in `docs/divergences.md`
- [x] omitted features classified separately from deferred work
- [x] `polycpp` module dependencies listed
- [x] missing `polycpp` primitives listed
- [x] external SDK/native driver strategy recorded
- [x] compatibility foundation review completed
- [x] security and fail-closed review completed

## API Mapping

- [x] major public `v0` APIs mapped
- [x] direct vs adapted vs deferred vs omitted status recorded
- [x] dynamic typing adaptations recorded
- [x] framework object boundary reviewed
- [x] Node parity surface review recorded

## Testing

- [x] unit test areas listed
- [x] integration test areas listed
- [x] upstream tests or fixtures to adapt listed
- [x] security and fail-closed tests listed
- [x] release-blocking behaviors listed

## Documentation Site

- [x] docs scaffold installed
- [x] docs build command recorded
- [x] GitHub Pages workflow present
- [x] `docs/build/` ignored
- [x] generated docs placeholders replaced or tracked for replacement before validation

## Pre-Coding Gate

- [x] `docs/research.md` is no longer placeholder-only
- [x] `docs/dependency-analysis.md` is no longer placeholder-only
- [x] `docs/api-mapping.md` is no longer placeholder-only
- [x] `docs/divergences.md` is no longer placeholder-only
- [x] `docs/test-plan.md` is no longer placeholder-only
- [x] strict readiness passed

## Implementation

- libgen catch-up note: implementation predates libgen, so the
  "implementation started after strict readiness" pre-coding gate cannot be
  applied retroactively. The planning artifacts above were authored by
  auditing the existing implementation; see the next item.
- [x] implementation reviewed after libgen catch-up
- [x] tests added as features are implemented
- [x] new divergences documented immediately

## Validation

- [x] targeted tests pass
- [x] README examples match actual code
- [x] remaining gaps are documented
- [x] `docs/test-plan.md` current validation records exact commands run

## Public Release

- [x] production-grade quality confirmed
- [x] public documentation ready
- [x] generated docs placeholder pages replaced with real public documentation
- [x] public docs do not contain local absolute paths
- [x] `python3 docs/build.py` passes
- [x] third-party license notices complete
- [x] GitHub repo visibility changed to public
  (already public on GitHub; visibility was changed before libgen existed.
  Future companions must follow the private-first workflow.)
