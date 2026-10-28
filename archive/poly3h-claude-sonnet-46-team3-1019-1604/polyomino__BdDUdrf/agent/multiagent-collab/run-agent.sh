#!/usr/bin/env bash
set +e
copilot_binary=/mnt/release-copilot/copilot
redact_arc_token='s/ARCCTL_SESSION_TOKEN=[^[:space:]"\\]+/ARCCTL_SESSION_TOKEN=<redacted>/g'
COPILOT_API_RETRY_PATTERN='(api_error_status.?40(1|9)|status.?code.?40(1|9)|http[^[:alnum:]]*40(1|9)|40(1|9)[^[:alnum:]]*(unauthori[sz]ed|too many requests)|unauthori[sz]ed|rate.?limit|too many requests|throttl|quota|invalid_grant|invalid.*token|expired.*token|refresh.*token|access.*token|oauth|please log in|not logged in)'

configure_copilot_command() {
  copilot_command=("$copilot_binary")
  context_flag=""
  if [ "${RELEASE_LONG_CONTEXT:-1}" = "1" ] && \
     "$copilot_binary" --no-auto-update --help 2>/dev/null | grep -Eq '^[[:space:]]*--context([[:space:]]|=|$)'; then
    context_flag="--context long_context"
  fi
}

append_copilot_telemetry() {
  local attempt_log_file="$1"
  local otel_file="$2"
  python3 -c "$trajectory_metrics_source" "$attempt_log_file" "$otel_file" || {
    echo "Copilot telemetry capture incomplete for $attempt_log_file; see its telemetry_error event" >&2
  }
}

extract_copilot_session_id() {
  local source_log="$1"
  python3 - "$source_log" <<'PY'
import json
import sys

session_id = ""
try:
    lines = open(sys.argv[1], errors="replace")
except OSError:
    lines = ()

for line in lines:
    try:
        event = json.loads(line)
    except json.JSONDecodeError:
        continue
    value = event.get("sessionId")
    if isinstance(value, str) and value:
        session_id = value
    data = event.get("data")
    if isinstance(data, dict):
        value = data.get("sessionId")
        if isinstance(value, str) and value:
            session_id = value

if session_id:
    print(session_id)
PY
}

has_recorded_arc_action() {
  local state_file="$1"
  python3 - "$state_file" <<'PY'
import json
import pathlib
import sys

def positive(value):
    try:
        return int(value) > 0
    except (TypeError, ValueError):
        return False

path = pathlib.Path(sys.argv[1])
if not path.exists():
    sys.exit(1)

try:
    data = json.loads(path.read_text(errors="replace"))
except (OSError, json.JSONDecodeError):
    sys.exit(1)

budget = data.get("action_budget")
if isinstance(budget, dict):
    for key in ("session_actions_used", "actions_used", "current_level_actions_used"):
        if positive(budget.get(key)):
            sys.exit(0)

level_clear_actions = data.get("level_clear_actions")
if isinstance(level_clear_actions, dict):
    for value in level_clear_actions.values():
        if positive(value):
            sys.exit(0)

last_action = data.get("last_action")
if isinstance(last_action, str) and last_action.startswith("ACTION"):
    sys.exit(0)

sys.exit(1)
PY
}

is_arc_runtime() {
  command -v arcctl >/dev/null 2>&1 \
    && arcctl --help 2>/dev/null | grep -q "ARC-AGI-3"
}

write_no_action_feedback() {
  local feedback_file="$1"
  cat > "$feedback_file" <<'EOF'
ENVIRONMENT FEEDBACK: Your turn ended, but this task is NOT finished and you still have many HOURS of compute budget remaining. Do not stop and do not conclude - treat this as a long-running optimization effort and keep going.

Your best submitted solution so far is already saved and safe. Your job now is to BEAT it. Hitting a plateau is not a reason to stop; it is a signal to try a structurally DIFFERENT approach.

Take another turn now. Do not answer only in prose - use tool calls (bash):
  - Re-read statement.txt and AGENT.md and your current /app/solution.cpp, and recall your best score so far.
  - Try a fundamentally different algorithm or strategy than the ones you have already tried, for example: a different problem/search formulation, a stronger metaheuristic (simulated annealing, tabu search, large-neighborhood search), an exact or ILP / branch-and-bound core, better constructive heuristics, smarter neighborhood moves, randomized restarts, or systematic parameter tuning.
  - Edit /app/solution.cpp and run `bash /app/submit.sh` to get a fresh graded score. Submit early and often; every improvement over your best counts.

Keep iterating and keep submitting for as long as you can make progress. Only stop if you have genuinely exhausted clearly distinct approaches. Continue working now.
EOF
}

append_no_action_feedback_event() {
  local log_file="$1"
  local attempt="$2"
  local max_attempts="$3"
  printf '{"type":"environment.feedback","data":{"event":"no_valid_arc_action","message":"Previous turn produced no valid ARC action; continuing the same Copilot session.","attempt":%s,"maxAttempts":%s}}\n' "$attempt" "$max_attempts" >> "$log_file"
}

append_optimization_continuation_event() {
  local log_file="$1"
  local attempt="$2"
  local max_attempts="$3"
  printf '{"type":"environment.feedback","data":{"event":"optimization_continuation","message":"Previous turn ended; continuing the same Copilot session for another optimization pass.","attempt":%s,"maxAttempts":%s}}\n' "$attempt" "$max_attempts" >> "$log_file"
}

is_retryable_copilot_api_error() {
  local attempt_log_file="$1"
  if grep -Eiq "$COPILOT_API_RETRY_PATTERN" "$attempt_log_file"; then
    return 0
  fi
  if [ -n "${copilot_log_dir:-}" ] && [ -d "$copilot_log_dir" ]; then
    local copilot_process_log
    for copilot_process_log in "$copilot_log_dir"/*.log; do
      [ -e "$copilot_process_log" ] || continue
      if grep -Eiq "$COPILOT_API_RETRY_PATTERN" "$copilot_process_log"; then
        return 0
      fi
    done
  fi
  return 1
}

copilot_api_retry_sleep_seconds() {
  local retry="$1"
  local base="${META_DUO_COPILOT_API_RETRY_BASE_SLEEP:-30}"
  local cap="${META_DUO_COPILOT_API_RETRY_MAX_SLEEP:-600}"
  case "$retry" in ''|*[!0-9]*) retry=1 ;; esac
  case "$base" in ''|*[!0-9]*) base=30 ;; esac
  case "$cap" in ''|*[!0-9]*) cap=600 ;; esac
  if [ "$retry" -lt 1 ]; then
    retry=1
  fi
  if [ "$base" -lt 1 ]; then
    base=30
  fi
  if [ "$cap" -lt "$base" ]; then
    cap="$base"
  fi
  local delay="$base"
  local step=1
  while [ "$step" -lt "$retry" ] && [ "$delay" -lt "$cap" ]; do
    delay=$((delay * 2))
    if [ "$delay" -gt "$cap" ]; then
      delay="$cap"
    fi
    step=$((step + 1))
  done
  if [ "$delay" -gt "$cap" ]; then
    delay="$cap"
  fi
  if [ "$delay" -lt "$cap" ]; then
    delay=$((delay + (RANDOM % base)))
    if [ "$delay" -gt "$cap" ]; then
      delay="$cap"
    fi
  fi
  echo "$delay"
}

append_copilot_api_retry_event() {
  local log_file="$1"
  local retry="$2"
  local max_retries="$3"
  local code="$4"
  local delay="$5"
  printf '{"type":"environment.feedback","data":{"event":"copilot_api_retry","message":"Copilot CLI returned a retryable 401/429/API error; backing off and resuming the same session instead of failing the run.","attempt":%s,"maxAttempts":%s,"exitCode":%s,"sleepSeconds":%s}}\n' "$retry" "$max_retries" "$code" "$delay" >> "$log_file"
}

append_copilot_api_retry_exhausted_event() {
  local log_file="$1"
  local max_retries="$2"
  local code="$3"
  printf '{"type":"environment.feedback","data":{"event":"copilot_api_retries_exhausted","message":"Copilot CLI kept returning retryable 401/429/API errors after all retry attempts.","maxAttempts":%s,"exitCode":%s}}\n' "$max_retries" "$code" >> "$log_file"
}

run_agent() {
  idx="$1"
  agent_id="$2"
  agent_count="$3"
  shift 3
  session_dir="$1"
  log_file="$2"
  model="$3"
  shared_prompt_path="$4"
  instruction="$5"
  : "${ARCCTL_SESSION_TOKEN:?missing ARC session token}"
  prompt_file="$session_dir/copilot-prompt.txt"
  copilot_log_dir="$session_dir/copilot-logs"
  trajectory_metrics_source='#!/usr/bin/env python3
"""Capture local Copilot OpenTelemetry records in a JSONL trajectory."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CaptureResult:
    records: int
    errors: int


def _append_event(handle, event_type: str, data: dict[str, object]) -> None:
    handle.write(
        json.dumps(
            {"type": event_type, "data": data},
            ensure_ascii=True,
            sort_keys=True,
        )
        + "\n"
    )


def append_copilot_otel_events(
    trajectory_path: str | Path,
    otel_path: str | Path,
) -> CaptureResult:
    """Append raw local OTel records to a Copilot trajectory as typed events."""
    trajectory = Path(trajectory_path)
    otel = Path(otel_path)
    source = otel.name
    records = 0
    errors = 0

    with trajectory.open("a", encoding="utf-8") as output:
        if not otel.exists():
            _append_event(
                output,
                "copilot.telemetry_error",
                {"reason": "otel_file_missing", "source": source},
            )
            return CaptureResult(records=0, errors=1)

        with otel.open(errors="replace") as telemetry:
            for line_number, line in enumerate(telemetry, start=1):
                if not line.strip():
                    continue
                try:
                    record = json.loads(line)
                except json.JSONDecodeError:
                    errors += 1
                    _append_event(
                        output,
                        "copilot.telemetry_error",
                        {
                            "line": line_number,
                            "reason": "invalid_otel_json",
                            "source": source,
                        },
                    )
                    continue
                records += 1
                _append_event(
                    output,
                    "copilot.telemetry",
                    {"record": record, "source": source},
                )

        if records == 0:
            errors += 1
            _append_event(
                output,
                "copilot.telemetry_error",
                {"reason": "otel_file_empty", "source": source},
            )
        _append_event(
            output,
            "copilot.telemetry_capture",
            {
                "errorCount": errors,
                "recordCount": records,
                "source": source,
            },
        )

    return CaptureResult(records=records, errors=errors)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trajectory", type=Path)
    parser.add_argument("otel", type=Path)
    args = parser.parse_args()
    result = append_copilot_otel_events(args.trajectory, args.otel)
    return 0 if result.records > 0 and result.errors == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
'
  mkdir -p "$session_dir" "$copilot_log_dir"
  configure_copilot_command
  config_code=$?
  if [ "$config_code" -ne 0 ]; then
    exit "$config_code"
  fi
  state_file="/logs/agent/arc_agi3_runs/$agent_id.state.json"
  is_arc_task=0
  if is_arc_runtime; then
    is_arc_task=1
  fi
  non_arc_continuation=1
  {
    printf %s 'You are one agent of many concurrent agents solving a task together. Follow this coordination protocol exactly.

'
    cat "$shared_prompt_path"
    printf '\n\nTask instruction:\n%s\n' "$instruction"
  } > "$prompt_file"
  # Startup jitter to spread Copilot model-load API hits across concurrent workers
  sleep "$(awk 'BEGIN{srand(); print int(rand()*30)}')"

  invoke_copilot() {
    local attempt_prompt_file="$1"
    local attempt_log_file="$2"
    local otel_file="${attempt_log_file%.jsonl}.otel"
    shift 2
    : > "$otel_file"
    AGENT_ID="$agent_id" AGENT_INDEX="$idx" AGENT_COUNT="$agent_count" COPILOT_ALLOW_ALL=true COPILOT_AUTO_UPDATE=false \
      COPILOT_OTEL_ENABLED=true COPILOT_OTEL_EXPORTER_TYPE=file \
      COPILOT_OTEL_FILE_EXPORTER_PATH="$otel_file" \
      OTEL_INSTRUMENTATION_GENAI_CAPTURE_MESSAGE_CONTENT=false \
      COPILOT_AGENT_ERROR_CODES_TO_RETRY="${COPILOT_AGENT_ERROR_CODES_TO_RETRY:-401,429,500,502,503,504,529}" \
      "${copilot_command[@]}" --model "$model" $(case "$model" in auto|*haiku*) ;; *) printf %s '--effort max' ;; esac) $context_flag --output-format json --allow-all \
      --allow-all-paths --allow-all-urls --no-custom-instructions \
      --available-tools=bash --available-tools=read_bash --available-tools=write_bash --available-tools=stop_bash --available-tools=view --available-tools=glob --available-tools=grep --available-tools=rg --available-tools=apply_patch --excluded-tools=task --excluded-tools=read_agent --excluded-tools=write_agent --excluded-tools=list_agents \
      --disable-builtin-mcps --secret-env-vars=COPILOT_GITHUB_TOKEN,COPILOT_PROVIDER_API_KEY,COPILOT_PROVIDER_BEARER_TOKEN \
      --stream off --silent --no-auto-update \
      --log-dir "$copilot_log_dir" "$@" -p "$(cat "$attempt_prompt_file")" 2>&1 | sed -E "$redact_arc_token; s|$ARCCTL_SESSION_TOKEN|<arc-token-redacted>|g" > "$attempt_log_file"
    local code=${PIPESTATUS[0]}
    append_copilot_telemetry "$attempt_log_file" "$otel_file"
    return "$code"
  }

  run_copilot_with_api_retries() {
    local attempt_prompt_file="$1"
    local attempt_log_base="$2"
    local max_api_retries="${META_DUO_COPILOT_API_RETRIES:-288}"
    case "$max_api_retries" in ''|*[!0-9]*) max_api_retries=288 ;; esac
    local retry=0
    local code=0
    while true; do
      local attempt_log_file="$attempt_log_base"
      if [ "$retry" -gt 0 ]; then
        attempt_log_file="${attempt_log_base%.jsonl}-api-retry-$retry.jsonl"
      fi
      if [ -n "$session_id" ]; then
        invoke_copilot "$attempt_prompt_file" "$attempt_log_file" --resume="$session_id"
      else
        invoke_copilot "$attempt_prompt_file" "$attempt_log_file"
      fi
      code=$?
      cat "$attempt_log_file" >> "$log_file"
      new_session_id="$(extract_copilot_session_id "$attempt_log_file")"
      if [ -n "$new_session_id" ]; then
        session_id="$new_session_id"
      fi
      if [ "$code" -eq 0 ]; then
        return 0
      fi
      if ! is_retryable_copilot_api_error "$attempt_log_file"; then
        return "$code"
      fi
      if [ "$retry" -ge "$max_api_retries" ]; then
        append_copilot_api_retry_exhausted_event "$log_file" "$max_api_retries" "$code"
        return "$code"
      fi
      retry=$((retry + 1))
      delay="$(copilot_api_retry_sleep_seconds "$retry")"
      append_copilot_api_retry_event "$log_file" "$retry" "$max_api_retries" "$code" "$delay"
      sleep "$delay"
    done
  }

  : > "$log_file"
  session_id=""
  submissions_file="/logs/agent/submissions.jsonl"
  attempt_log="$session_dir/copilot-attempt-0.jsonl"
  run_copilot_with_api_retries "$prompt_file" "$attempt_log"
  code=$?
  if [ "$code" -ne 0 ]; then
    exit "$code"
  fi
  if [ "$is_arc_task" -eq 1 ]; then
    if has_recorded_arc_action "$state_file"; then
      exit 0
    fi
  elif [ "$non_arc_continuation" -ne 1 ]; then
    exit 0
  fi

  max_no_action_reprompts=1000
  # Retain the original submission-log probe and its shell output behavior.
  prev_submissions=$(wc -l < "$submissions_file" 2>/dev/null || echo 0)
  case "$prev_submissions" in ''|*[!0-9]*) prev_submissions=0 ;; esac
  no_action_attempt=1
  while [ "$no_action_attempt" -le "$max_no_action_reprompts" ]; do
    feedback_file="$session_dir/copilot-no-action-feedback-$no_action_attempt.txt"
    attempt_log="$session_dir/copilot-attempt-$no_action_attempt.jsonl"
    write_no_action_feedback "$feedback_file"
    if [ "$is_arc_task" -eq 1 ]; then
      append_no_action_feedback_event "$log_file" "$no_action_attempt" "$max_no_action_reprompts"
    else
      append_optimization_continuation_event "$log_file" "$no_action_attempt" "$max_no_action_reprompts"
    fi
    run_copilot_with_api_retries "$feedback_file" "$attempt_log"
    code=$?
    if [ "$code" -ne 0 ]; then
      exit "$code"
    fi
    if [ "$is_arc_task" -eq 1 ]; then
      if has_recorded_arc_action "$state_file"; then
        exit 0
      fi
    fi
    no_action_attempt=$((no_action_attempt + 1))
  done

  if [ "$is_arc_task" -eq 1 ]; then
    printf '{"type":"environment.feedback","data":{"event":"no_valid_arc_action_reprompts_exhausted","message":"No valid ARC action was recorded after all environment feedback attempts.","maxAttempts":%s}}\n' "$max_no_action_reprompts" >> "$log_file"
    exit 1
  fi
  printf '{"type":"environment.feedback","data":{"event":"optimization_continuations_completed","message":"Completed all configured optimization continuation passes.","maxAttempts":%s}}\n' "$max_no_action_reprompts" >> "$log_file"
  exit 0
}

run_agent "$@"
