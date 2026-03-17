# q3now TODOs

All current TODOs have been completed. See git history for context.

## Completed

- **GitHub Actions CI** — `.github/workflows/build.yml` customized:
  `BUILD_GAME_QVMS=OFF` on all jobs, `main`→`master` for code-signing gate,
  game module verification step on Linux and macOS.

- **Quarterly upstream sync** — `sync-upstream.sh` created. Run it to fetch
  and merge upstream ioquake3 changes into a dated branch for PR review.

- **pak0.pk3 packaging** — `make pak` packages `modfiles/` into
  `build/baseq3/pak0.pk3`. `make install` copies it alongside game modules.
