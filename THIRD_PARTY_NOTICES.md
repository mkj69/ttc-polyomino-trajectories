The Harbor viewer in `docs/harbor/assets/`, `docs/harbor/fonts/`, and its
favicon files is redistributed from the installed **Harbor 0.24.0** package.
Harbor is copyright its contributors and is licensed under Apache License 2.0;
the complete license is in [licenses/Harbor-Apache-2.0.txt](licenses/Harbor-Apache-2.0.txt).
Upstream project: <https://github.com/laude-institute/harbor>.

Changes to the packaged viewer: asset and font paths are rebased under
`/ttc-polyomino-trajectories/harbor/`; the React Router basename is adjusted;
application startup waits for the read-only service worker; a snapshot banner
and read-only UI stylesheet are added. React, React Router, Radix UI, TanStack
Query, Tailwind, Shiki and icon/font dependencies remain part of the upstream
compiled application and retain their respective upstream licenses and any
notices embedded in the distributable. This repository's MIT license applies
to its original adapter, overview and scripts, not to the vendored viewer.

Supplemental original license texts for 294 runtime dependency packages are
retained in [licenses/Frontend-dependency-notices.md](licenses/Frontend-dependency-notices.md).
They were collected from a resolution of the upstream viewer dependency
declaration on the preparation date, and are not asserted to be the exact
lockfile used to build the installed wheel. The Google Sans Code font's
SIL Open Font License is in [licenses/Google-Sans-Code-OFL.txt](licenses/Google-Sans-Code-OFL.txt).

The snapshot design was studied at <https://realtimeagents.org/harbor/> by
reading `snapshot-bootstrap.js`, `snapshot-worker.js`, and `rtbench-viewer.js`.
Those files are not redistributed here; the TTC adapter is original code.
No Realtime-Bench dataset, video, recording-player code, or branding is included.

Source trajectories can contain agent-created code, benchmark instructions,
and references to third-party software. Their original notices are retained
where present; the data license does not override third-party terms.
