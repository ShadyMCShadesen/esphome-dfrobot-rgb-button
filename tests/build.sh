#!/usr/bin/env bash
# Validate or compile one example for one target, using the component from this checkout.
#
#   tests/build.sh <example> <target> [config|compile]
#   tests/build.sh minimal esp8266 compile
#
# Examples live in examples/*.yaml, targets in tests/targets/*.yaml.
set -euo pipefail

example="${1:?usage: tests/build.sh <example> <target> [config|compile]}"
target="${2:?usage: tests/build.sh <example> <target> [config|compile]}"
command="${3:-config}"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${root}/.build/${target}"
mkdir -p "${build_dir}"

# The examples pull the component from GitHub; tests must use the local checkout instead.
sed "s|source: github://ShadyMCShadesen/esphome-dfrobot-rgb-button@main|source: ${root}/components|" \
  "${root}/examples/${example}.yaml" >"${build_dir}/example.yaml"
if grep -q "github://ShadyMCShadesen" "${build_dir}/example.yaml"; then
  echo "error: ${example}.yaml still references the GitHub source" >&2
  exit 1
fi
cp "${root}/tests/targets/${target}.yaml" "${build_dir}/${example}.yaml"
cp "${root}/tests/secrets.yaml" "${build_dir}/secrets.yaml"

exec esphome "${command}" "${build_dir}/${example}.yaml"
