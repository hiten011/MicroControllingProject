#!/usr/bin/env bash
# Run the AMU B1/wheeldrop fault-stop TLC check.
# Usage:
#   run-check.sh           # expect PASS on RobotFaultStop
#   run-check.sh --broken  # expect FAIL (invariant violation) on RobotFaultStopBroken
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FORMAL="${ROOT}/formal"
HELPERS="$(cd "$(dirname "$0")" && pwd)"
JAR="${HELPERS}/tla2tools.jar"
TLA_URL="${TLA_URL:-https://github.com/tlaplus/tlaplus/releases/download/v1.7.3/tla2tools.jar}"
MODE="good"

if [[ "${1:-}" == "--broken" ]]; then
  MODE="broken"
elif [[ -n "${1:-}" ]]; then
  echo "usage: $0 [--broken]" >&2
  exit 2
fi

if [[ ! -f "${JAR}" ]]; then
  echo "Downloading tla2tools.jar (v1.7.3) -> ${JAR}"
  curl -fsSL -o "${JAR}" "${TLA_URL}"
fi

if [[ "${MODE}" == "good" ]]; then
  SPEC="RobotFaultStop"
  echo "TLC ${SPEC} (expect success)"
  java -XX:+UseParallelGC -cp "${JAR}" tlc2.TLC -config "${FORMAL}/${SPEC}.cfg" "${FORMAL}/${SPEC}.tla"
else
  SPEC="RobotFaultStopBroken"
  echo "TLC ${SPEC} (expect invariant violation)"
  set +e
  out="$(java -XX:+UseParallelGC -cp "${JAR}" tlc2.TLC -config "${FORMAL}/${SPEC}.cfg" "${FORMAL}/${SPEC}.tla" 2>&1)"
  rc=$?
  set -e
  printf '%s\n' "${out}"
  if printf '%s\n' "${out}" | grep -q "Invariant SafeMotors is violated"; then
    echo "run-check: broken variant failed as expected (SafeMotors violated)"
    exit 0
  fi
  echo "run-check: broken variant did not report SafeMotors violation (tlc exit ${rc})" >&2
  exit 1
fi
