# TTC polyomino trajectories

A public, sanitized archive of 33 polyomino-packing runs from a reproduction
of Test-Time Communication (TTC, Park et al.). The static website lets readers
inspect score curves, per-agent activity, graded submissions, and complete
semantic trajectories in the Harbor viewer.

**Browse:** <https://mkj69.github.io/ttc-polyomino-trajectories/>
(the expected URL after the repository owner publishes GitHub Pages).

## Experiment

Agents develop a C++ polyomino packing solution, evaluated on 70 test cases.
The judge returns a reward in [0, 1] and per-case submission feedback.
The harness uses Harbor with GitHub Copilot CLI 1.0.70. Runs have a 3-hour
wall-clock budget; the archive also includes one 12-hour Sonnet run per protocol.

| Protocol | Agents | Working arrangement |
| --- | ---: | --- |
| `solo` | 1 | One agent works on the graded solution. |
| `team3` | 3 | Official antiherding protocol; all agents share `/app/solution.cpp` and collaboration logs. |
| `iso3` | 3 | Our `antiherding_isolated` protocol; private working solutions and a lock-guarded promotion of strictly better candidates to the graded file. |

The scientific question is whether team gains reflect knowledge transfer,
relay or assembly across agents, or simply additional sampling and compute.
The archive exposes evidence for inspection; it does not establish a causal
communication benefit. The broader study's output-token-matched analysis
finds no team advantage; that budget analysis is supplied context and is not
reimplemented by this viewer.

## Results

Three hours, five independent runs per model/protocol. Final graded reward,
mean ± sample standard deviation (`ddof=1`), recomputed from `result.json`:

| Model | Solo | Team 3 | Isolated 3 | Best@3-of-solo* |
| --- | ---: | ---: | ---: | ---: |
| Claude Sonnet 4.6 | 0.767 ± 0.051 | 0.812 ± 0.048 | 0.812 ± 0.028 | 0.819 |
| gpt-6.1-sol | 0.983 ± 0.005 | 0.983 ± 0.008 | 0.982 ± 0.008 | 0.986 |

*Best@3 values are supplied study results, not recalculated here. Twelve-hour
Sonnet final rewards: solo 0.783, team3 0.842, iso3 0.8315, one run each.
The website displays the full archived precision for per-run rewards.

The running best submitted score can differ from the final reward: the final
judge evaluates the solution file left at the end of the run, while a submission
may evaluate a private candidate or a solution later overwritten.
Harbor can display `AgentTimeoutError` when the wall-clock budget expires;
the final verifier reward is still present for all 33 archived runs.

## Browse and download

- The **overview** filters by model, protocol, budget, and replicate. Each run
  has a team-best curve, per-agent best curves, agent activity lanes, a searchable
  and paginated event index, and links to sanitized source streams.
- **Harbor viewer** opens the trial's trajectory, tool calls/results, verifier
  output, and files. Agent and session labels make the merged timeline explicit.
- `jobs/` is the sanitized text archive, including original Copilot JSONL and
  OTel records, prompts, submissions, collaboration logs, source snapshots, job
  configuration/results, and verifier output. Large text files end in `.gz`.
  Use Python's `gzip` module or `gzip -dc file.jsonl.gz` to read them.
- `docs/archive/` mirrors sanitized trial files so GitHub Pages can serve
  downloads without a backend. The two paths use identical Git blobs.
- `docs/harbor/snapshots/` contains compressed native API responses, including
  the generated ATIF. `docs/data/` contains overview data.
- `metadata/` records omissions, redaction counts, snapshot construction,
  derived counts, and the latest sensitive-data scan. No secret values or
  gateway hostnames are included in those manifests.

Original event streams include redundant streaming deltas. Harbor's ATIF is a
**derived viewing projection**, merging completed user/assistant messages,
completed reasoning, tool requests/results, and errors by timestamp. Every
step identifies its agent, session, source file, source line and original event
type. Streaming deltas and telemetry are omitted from ATIF, retained in source
logs. Source-truncated JSONL fragments are retained in explicit
`archive.unparsed_record` wrappers, with affected files/lines listed in the
export manifest; no completed record is silently dropped. Tool results are attached to their originating call even when another
agent's events occurred before completion. Unpaired tool-result counts, if
any, are disclosed in each run's metadata. No transcript text is synthesized.

Shared-log reads/writes are **heuristics** over commands referencing
`findings.log`, `adoptions.log`, `disconfirmations.log`, or `champion.log`.
They do not verify command success, actual file access, comprehension, adoption,
or causation; commands can mix reads and writes. Overview command previews
are limited to 240 characters; Harbor and original logs retain full commands.
Times are minutes since `agent_execution.started_at`; naive timestamps are
interpreted as UTC. Tool dispatch counts include observed failed calls.

## Run the static site locally

No Node installation, build, API key, or live Harbor process is required to
serve the finished website. Python 3 is sufficient. From this repository:

```sh
mkdir -p .preview
ln -s ../docs .preview/ttc-polyomino-trajectories
python3 -m http.server 8080 --bind 127.0.0.1 --directory .preview
```

Open <http://127.0.0.1:8080/ttc-polyomino-trajectories/>. For a repeated setup,
reuse the existing symlink. The native viewer is at the `harbor/` subpath.
Use a current browser with service workers and `DecompressionStream`.
Service workers require HTTPS or localhost; opening `index.html` via `file://`
does not work. The overview does not need a service worker.

## How the snapshot was made

The reference design is <https://realtimeagents.org/harbor/>. Its three adapter
scripts were downloaded and inspected as data, not executed or redistributed.
Our adapter uses the installed Harbor frontend and intercepts read-only API
requests with a service worker backed by compressed static JSON. No live API
server is contacted by the published viewer. Write methods return HTTP 405;
login, run creation, deletion, uploads, analysis generation and editor actions
are hidden. Cross-job comparison is outside this snapshot's supported UI.

The task's environment description said Harbor 0.23.0; installed package
metadata reports **0.24.0**, which is the version actually used. We reused its
prebuilt viewer. Only asset paths, Router basename, startup gating, and a
read-only banner/style were changed. No frontend rebuild was needed.

To reproduce from a private raw source tree (the raw path is deliberately not
hard-coded into these public scripts):

```sh
python3 scripts/test_redaction.py
python3 scripts/redact.py /path/to/private/jobs
python3 scripts/build_data.py
harbor view jobs --host 127.0.0.1 --port 8765 --jobs --no-build
```

In another terminal, using the same Harbor installation's static directory:

```sh
python3 scripts/snapshot.py --url http://127.0.0.1:8765 \
  --static /path/to/harbor/site-packages/harbor/viewer/static
sh scripts/scan.sh
```

Run `snapshot.py` only against **sanitized** jobs. It calls GET endpoints only,
enumerating the job list/filters/configuration, per-job tasks/trials/chart data,
and each trial's result, trajectory, files, verifier output, artifacts, agent
logs, interaction data, recording metadata and navigation. Authentication
state is synthesized as logged out; no account state is read. The generated
`trajectory.json` files are gzipped after HTTP capture. If rerunning the
capture without `build_data.py`, first restore those derived JSON files with
`gzip -dk jobs/*/polyomino__*/agent/trajectory.json.gz`.

The export selects exactly 30 three-hour runs and three twelve-hour Sonnet
runs; outage/smoke jobs are excluded. Files above 200 KB are generally gzipped;
job/trial configuration and results stay plain JSON. Files approaching the
50 MB limit can be losslessly chunked with a parts index. The included export
manifests identify any such cases and every omitted binary/non-text artifact.

## Redaction and verification

`scripts/redact.py` never reads credential files or prints matches. It discovers
network authorities from archived URL strings and process endpoint variables;
all archived URL authorities and discovered bare host spellings become
`<gateway>`. Private-host/IP patterns are also anonymized. Credential
assignments, common API/GitHub/AWS/JWT token patterns, bearer values and exact
process-environment secrets become `<redacted>`. Opaque reasoning payloads
become `<omitted-opaque>`. ELF binaries, serialized objects, non-UTF8 files,
and symbolic links are excluded. Text redaction can change code lengths;
`code_chars` is original judge metadata, not a sanitized-file length check.

`scripts/scan.sh` fails on recognized residual credentials, unredacted archived
authorities, invalid JSON/JSONL/gzip, unexpected binaries/symlinks, individual
files ≥50 MB, or a tree ≥1.5 GB (excluding Git). It examines decompressed
payloads and exact process-env values without printing them. This is a
pattern-based scan, not proof that arbitrary unknown/encoded secrets cannot
exist. Nine synthetic regression tests include nested escaped JSON,
source-truncation preservation, and fail-closed checks for damaged valid JSON,
gzipped secrets and unredacted authorities.

Optional browser integration checks require Playwright installed outside this
repository. Set `PLAYWRIGHT_MODULE` to its `index.mjs`, `QA_OUTPUT` to a writable
evidence directory, and invoke:

```sh
node scripts/check_site.mjs http://127.0.0.1:8080/ttc-polyomino-trajectories/
```

The checks cover desktop/mobile layout, filters and event inspection, a fresh
Harbor deep link, static API pagination and file access, rejected writes, and
all 33 trajectory responses. `scripts/figures.py` optionally creates a static
reward plot with matplotlib/numpy.

## Publish on GitHub Pages

After owner review, push this local repository to a public repository named
`ttc-polyomino-trajectories`. In **Settings → Pages**, choose **Deploy from a
branch**, branch **main**, folder **/docs**, and save. `.nojekyll` is included.
Known job/trial routes have materialized `index.html` files, so fresh deep
links work without first visiting the home page. A 404 fallback handles
additional native routes. No workflow, server secrets, or Pages build is needed.

The native viewer is explicitly mounted at
`/ttc-polyomino-trajectories/harbor/`. A different repository name requires
changing that prefix in the adapter/exporter and regenerating the snapshot.
The overview and data URLs are relative. The public URL above is an intended
destination, not a claim that this local-only preparation has been published.

## License and attribution

Original overview code, adapter and scripts: **MIT**, see [LICENSE](LICENSE).
Experiment data and derived data: **CC BY 4.0**, attribution **Kaijing Ma,
TTC polyomino reproduction trajectories, 2026**; see [DATA_LICENSE.md](DATA_LICENSE.md).
The vendored Harbor viewer is **Apache 2.0**, not covered by our MIT license.
Third-party task text, dependencies and quoted material retain their original
terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
