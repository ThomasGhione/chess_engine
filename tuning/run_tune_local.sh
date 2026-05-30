#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"
export PATH="${script_dir}:${PATH}"
export MPLCONFIGDIR="${MPLCONFIGDIR:-/tmp/hydray-matplotlib-cache}"

config="${1:-tuning_config.json}"
if [[ $# -gt 0 && ( "${config}" == *.json || -f "groups/${config}.json" || -f "groups/${config}" ) ]]; then shift; else config="tuning_config.json"; fi
if [[ "${config}" != */* && -f "groups/${config}.json" ]]; then config="groups/${config}.json"; fi
if [[ "${config}" != */* && -f "groups/${config}" ]]; then config="groups/${config}"; fi
if [[ "${config}" == groups/* ]]; then
  python3 -c 'import json,sys; b=json.load(open("base_config.json")); g=json.load(open(sys.argv[1])); b.update(g); json.dump(b, open("tuning_config.json","w"), indent=2); print("Using", sys.argv[1])' "${config}"
  config="tuning_config.json"
fi

data_path="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("data_path", "data.npz"))' "${config}")"
model_path="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("model_path", "model.pkl"))' "${config}")"

state_dir=".tuning_state"
mkdir -p "${state_dir}"
read -r state_id config_hash < <(python3 -c 'import hashlib,json,sys; c=json.load(open(sys.argv[1])); key=str(c.get("data_path","data.npz"))+"|"+str(c.get("model_path","model.pkl")); body=json.dumps(c,sort_keys=True,separators=(",",":")); print(hashlib.sha256(key.encode()).hexdigest()[:16], hashlib.sha256(body.encode()).hexdigest())' "${config}")
state_file="${state_dir}/${state_id}"
if [[ ! -f "${state_file}" || "$(cat "${state_file}")" != "${config_hash}" ]]; then
  [[ " $* " == *" --no-resume "* ]] || set -- --no-resume "$@"
  [[ " $* " == *" --no-fast-resume "* ]] || set -- --no-fast-resume "$@"
  echo "New or changed tuning dataset: forcing no-resume."
fi
printf '%s\n' "${config_hash}" > "${state_file}"

log_path="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("log_path", "log.txt"))' "${config}")"

exec tune local -c "${config}" -d "${data_path}" --model-path "${model_path}" -l "${log_path}" "$@"
