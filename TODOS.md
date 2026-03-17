# q3now TODOs

All current TODOs have been completed. See git history for context.

## Completed

- **GitHub Actions CI** — `.github/workflows/build.yml` customized:
  QVM builds enabled on all jobs, `main`→`master` for code-signing gate,
  game module verification step on Linux and macOS (both native dylibs and QVMs).

- **Quarterly upstream sync** — `sync-upstream.sh` created. Run it to fetch
  and merge upstream ioquake3 changes into a dated branch for PR review.

- **QVM packaging** — `make pak` packages `modfiles/` + custom QVMs into
  `build/baseq3/zz-q3now.pk3`. The `zz-` prefix ensures this pak loads last,
  overriding the stock 1999 QVMs in `pak0.pk3`.

- **Build verification** — `make check` verifies QVMs, dylibs, and pk3 contents
  after a build. Guards against the silent failure of stock QVMs loading instead
  of custom ones.

- **Developer debug mode** — `make run-dev` launches with `vm_*=0` so the engine
  loads native `.dylib` modules instead of QVMs. Gives real crash stack traces.
