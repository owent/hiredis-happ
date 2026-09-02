#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
WORK_ROOT="${HIREDIS_HAPP_TEST_REDIS_ROOT:-${REPO_ROOT}/build_jobs_redis_fixture}"
DOWNLOAD_URL="${HIREDIS_HAPP_TEST_REDIS_DOWNLOAD_URL:-https://download.redis.io/redis-stable.tar.gz}"
DOWNLOAD_DIR="${WORK_ROOT}/download"
SOURCE_DIR="${WORK_ROOT}/src"
INSTALL_DIR="${WORK_ROOT}/install"
RUNTIME_DIR="${WORK_ROOT}/runtime"
SINGLE_HOST="${HIREDIS_HAPP_TEST_SINGLE_HOST:-127.0.0.1}"
SINGLE_PORT="${HIREDIS_HAPP_TEST_SINGLE_PORT:-6390}"
CLUSTER_HOST="${HIREDIS_HAPP_TEST_CLUSTER_HOST:-127.0.0.1}"
CLUSTER_BASE_PORT="${HIREDIS_HAPP_TEST_CLUSTER_BASE_PORT:-7300}"
CLUSTER_PORT="${HIREDIS_HAPP_TEST_CLUSTER_PORT:-${CLUSTER_BASE_PORT}}"
CLUSTER_REPLICAS="${HIREDIS_HAPP_TEST_CLUSTER_REPLICAS:-1}"
CLUSTER_NODE_COUNT="${HIREDIS_HAPP_TEST_CLUSTER_NODE_COUNT:-6}"
REDIS_PROVIDER="${HIREDIS_HAPP_TEST_REDIS_PROVIDER:-auto}"
REDIS_IMAGE="${HIREDIS_HAPP_TEST_REDIS_IMAGE:-redis:8-alpine}"
DOCKER_PREFIX="${HIREDIS_HAPP_TEST_REDIS_DOCKER_PREFIX:-hiredis-happ-redis}"

ARCHIVE_NAME="$(basename "${DOWNLOAD_URL}")"
ARCHIVE_PATH="${DOWNLOAD_DIR}/${ARCHIVE_NAME}"
REDIS_SERVER="${INSTALL_DIR}/bin/redis-server"
REDIS_CLI="${INSTALL_DIR}/bin/redis-cli"

usage() {
  cat <<'EOF'
Usage: test/redis/redis-fixture.sh <command>

Commands:
  download            Download the official Redis source archive (source provider).
  build               Provision redis-server/redis-cli for the resolved provider (source: compile; homebrew: brew install).
  prepare             Same as build.
  start-single        Start a standalone Redis server for raw integration tests.
  stop-single         Stop the standalone Redis server.
  restart-single      Restart the standalone Redis server.
  start-cluster       Start a temporary 3 master + 3 replica Redis Cluster.
  stop-cluster        Stop the Redis Cluster nodes.
  restart-cluster     Restart the Redis Cluster nodes.
  start-all           Start both standalone and cluster fixtures.
  stop-all            Stop standalone and cluster fixtures.
  cleanup             Stop everything and remove runtime data.
  print-env           Print the environment variables used by the integration tests.
  status              Show fixture status.

Providers (HIREDIS_HAPP_TEST_REDIS_PROVIDER, default: auto):
  auto                Use docker on Linux when the Docker daemon is reachable, the Homebrew
                      package on macOS when brew is available, otherwise build from source.
  docker              Run Redis/Redis Cluster in Docker containers (requires Docker with host networking, i.e. Linux).
  homebrew            Use the precompiled redis formula from Homebrew (macOS or Linuxbrew).
  source              Download and build the official Redis source archive.

Docker tunables:
  HIREDIS_HAPP_TEST_REDIS_IMAGE         Image used for containers (default: redis:8-alpine).
  HIREDIS_HAPP_TEST_REDIS_DOCKER_PREFIX Container name prefix (default: hiredis-happ-redis).
EOF
}

die() {
  echo "[redis-fixture] $*" >&2
  exit 1
}

log() {
  echo "[redis-fixture] $*" >&2
}

detect_make_jobs() {
  if [[ -n "${HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS:-}" ]]; then
    echo "${HIREDIS_HAPP_TEST_REDIS_BUILD_JOBS}"
    return
  fi

  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi

  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN
    return
  fi

  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
    return
  fi

  echo 2
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"
}

ensure_directories() {
  mkdir -p "${DOWNLOAD_DIR}" "${SOURCE_DIR}" "${INSTALL_DIR}" "${RUNTIME_DIR}"
}

fetch_archive() {
  ensure_directories
  if [[ -f "${ARCHIVE_PATH}" ]]; then
    log "Reuse downloaded archive: ${ARCHIVE_PATH}"
    return
  fi

  if command -v curl >/dev/null 2>&1; then
    log "Downloading Redis from ${DOWNLOAD_URL}"
    curl -L --fail --retry 3 --output "${ARCHIVE_PATH}" "${DOWNLOAD_URL}"
  elif command -v wget >/dev/null 2>&1; then
    log "Downloading Redis from ${DOWNLOAD_URL}"
    wget -O "${ARCHIVE_PATH}" "${DOWNLOAD_URL}"
  else
    die "Either curl or wget is required to download Redis"
  fi
}

extract_archive() {
  fetch_archive
  local top_level
  top_level="$(tar -tzf "${ARCHIVE_PATH}" | cut -d/ -f1 | sed '/^$/d' | sort -u | sed -n '1p')"
  [[ -n "${top_level}" ]] || die "Could not detect Redis source directory inside ${ARCHIVE_PATH}"

  if [[ -d "${SOURCE_DIR}/${top_level}" ]]; then
    log "Reuse extracted source tree: ${SOURCE_DIR}/${top_level}"
    echo "${SOURCE_DIR}/${top_level}"
    return
  fi

  log "Extracting ${ARCHIVE_PATH}"
  tar -xzf "${ARCHIVE_PATH}" -C "${SOURCE_DIR}"
  [[ -d "${SOURCE_DIR}/${top_level}" ]] || die "Extraction failed for ${ARCHIVE_PATH}"
  echo "${SOURCE_DIR}/${top_level}"
}

build_redis() {
  ensure_directories
  if [[ -x "${REDIS_SERVER}" && -x "${REDIS_CLI}" ]]; then
    log "Reuse built Redis tools under ${INSTALL_DIR}"
    return
  fi

  require_command make
  local src_dir
  local jobs
  src_dir="$(extract_archive)"
  jobs="$(detect_make_jobs)"

  log "Building Redis from ${src_dir}"
  make -C "${src_dir}" distclean >/dev/null 2>&1 || true
  make -C "${src_dir}" -j "${jobs}" MALLOC=libc BUILD_TLS=no
  make -C "${src_dir}" PREFIX="${INSTALL_DIR}" install

  [[ -x "${REDIS_SERVER}" && -x "${REDIS_CLI}" ]] || die "redis-server or redis-cli was not installed correctly"
}

docker_available() {
  command -v docker >/dev/null 2>&1 || return 1
  docker info >/dev/null 2>&1 || return 1
}

homebrew_resolve_redis_tools() {
  command -v brew >/dev/null 2>&1 || return 1
  local prefix
  prefix="$(brew --prefix redis 2>/dev/null || true)"
  if [[ -n "${prefix}" && -x "${prefix}/bin/redis-server" && -x "${prefix}/bin/redis-cli" ]]; then
    REDIS_SERVER="${prefix}/bin/redis-server"
    REDIS_CLI="${prefix}/bin/redis-cli"
    return 0
  fi
  return 1
}

homebrew_ensure_redis() {
  require_command brew
  if ! homebrew_resolve_redis_tools; then
    log "Installing Redis via Homebrew"
    brew install redis
    homebrew_resolve_redis_tools || die "redis-server or redis-cli not found after 'brew install redis'"
  fi
  log "Using Homebrew Redis: ${REDIS_SERVER}"
}

ensure_redis_tools() {
  if [[ "${PROVIDER}" == "homebrew" ]]; then
    homebrew_ensure_redis
  else
    build_redis
  fi
}

resolve_provider() {
  case "${REDIS_PROVIDER}" in
    docker | homebrew | source)
      echo "${REDIS_PROVIDER}"
      ;;
    auto)
      # Host networking is required so the host-side test binary can follow
      # cluster redirections to the announced addresses. Docker host
      # networking only works on Linux, so auto prefers the Homebrew
      # precompiled package on macOS and the source build elsewhere.
      if [[ "$(uname -s)" == "Linux" ]] && docker_available; then
        echo "docker"
      elif [[ "$(uname -s)" == "Darwin" ]] && command -v brew >/dev/null 2>&1; then
        echo "homebrew"
      else
        echo "source"
      fi
      ;;
    *)
      die "Unknown HIREDIS_HAPP_TEST_REDIS_PROVIDER: ${REDIS_PROVIDER} (expected auto, docker, homebrew or source)"
      ;;
  esac
}

docker_container_single() {
  echo "${DOCKER_PREFIX}-single"
}

docker_container_cluster() {
  local index="$1"
  echo "${DOCKER_PREFIX}-cluster-${index}"
}

docker_remove_container() {
  local name="$1"
  docker rm -f "${name}" >/dev/null 2>&1 || true
}

docker_wait_for_redis() {
  local container="$1"
  shift
  local attempts="$1"
  shift
  local i

  for ((i = 0; i < attempts; ++i)); do
    if docker exec "${container}" redis-cli "$@" PING >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done

  return 1
}

docker_start_single() {
  local name
  name="$(docker_container_single)"
  docker_remove_container "${name}"

  log "Starting standalone Redis container ${name} (${REDIS_IMAGE}) on ${SINGLE_HOST}:${SINGLE_PORT}"
  docker run -d --name "${name}" -p "${SINGLE_HOST}:${SINGLE_PORT}:6379" "${REDIS_IMAGE}" \
    redis-server --save '' --appendonly no --protected-mode no >/dev/null
  docker_wait_for_redis "${name}" 120 || die "Standalone Redis container did not become ready"
}

docker_stop_single() {
  docker_remove_container "$(docker_container_single)"
}

docker_start_cluster_nodes() {
  local index
  local port
  local name

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    port="$(cluster_node_port "${index}")"
    name="$(docker_container_cluster "${index}")"
    docker_remove_container "${name}"

    log "Starting Redis Cluster container ${name} (${REDIS_IMAGE}) on ${CLUSTER_HOST}:${port}"
    docker run -d --name "${name}" --network host "${REDIS_IMAGE}" \
      redis-server \
      --bind "${CLUSTER_HOST}" \
      --port "${port}" \
      --protected-mode no \
      --save '' \
      --appendonly no \
      --cluster-enabled yes \
      --cluster-config-file nodes.conf \
      --cluster-node-timeout 5000 \
      --cluster-announce-ip "${CLUSTER_HOST}" \
      --cluster-announce-port "${port}" \
      --cluster-announce-bus-port "$((port + 10000))" >/dev/null
  done

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    docker_wait_for_redis "$(docker_container_cluster "${index}")" 120 -h "${CLUSTER_HOST}" -p "$(cluster_node_port "${index}")" ||
      die "Redis Cluster container $(cluster_node_port "${index}") did not become ready"
  done
}

docker_create_cluster() {
  local nodes=()
  local index

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    nodes+=("${CLUSTER_HOST}:$(cluster_node_port "${index}")")
  done

  log "Creating Redis Cluster with ${CLUSTER_NODE_COUNT} container nodes"
  docker exec "$(docker_container_cluster 0)" redis-cli --cluster create "${nodes[@]}" \
    --cluster-replicas "${CLUSTER_REPLICAS}" --cluster-yes
}

docker_wait_for_cluster_ok() {
  local index
  local port

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    port="$(cluster_node_port "${index}")"
    if ! docker_wait_for_redis "$(docker_container_cluster "${index}")" 120 -h "${CLUSTER_HOST}" -p "${port}"; then
      return 1
    fi
  done

  for ((index = 0; index < 100; ++index)); do
    if docker exec "$(docker_container_cluster 0)" redis-cli -h "${CLUSTER_HOST}" -p "${CLUSTER_PORT}" cluster info 2>/dev/null |
      grep -q '^cluster_state:ok'; then
      return 0
    fi
    sleep 0.2
  done

  return 1
}

docker_start_cluster() {
  [[ "${CLUSTER_NODE_COUNT}" -eq 6 ]] || die "This fixture currently expects 6 cluster nodes"
  [[ "${CLUSTER_REPLICAS}" -eq 1 ]] || die "This fixture currently expects --cluster-replicas 1"

  docker_stop_cluster || true
  docker_start_cluster_nodes
  docker_create_cluster
  docker_wait_for_cluster_ok || die "Redis Cluster did not reach cluster_state:ok"
}

docker_stop_cluster() {
  local index
  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    docker_remove_container "$(docker_container_cluster "${index}")"
  done
}

wait_for_redis() {
  local host="$1"
  local port="$2"
  local attempts="${3:-100}"
  local i

  if [[ ! -x "${REDIS_CLI}" ]]; then
    return 1
  fi

  for ((i = 0; i < attempts; ++i)); do
    if "${REDIS_CLI}" -h "${host}" -p "${port}" PING >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done

  return 1
}

wait_for_pid_exit() {
  local pid="$1"
  local attempts="${2:-30}"
  local i

  for ((i = 0; i < attempts; ++i)); do
    if ! kill -0 "${pid}" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done

  return 1
}

shutdown_redis() {
  local host="$1"
  local port="$2"
  local pid_file="$3"

  if [[ -x "${REDIS_CLI}" ]]; then
    "${REDIS_CLI}" -h "${host}" -p "${port}" shutdown nosave >/dev/null 2>&1 || true
  fi

  if [[ -f "${pid_file}" ]]; then
    local pid
    pid="$(cat "${pid_file}")"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" >/dev/null 2>&1; then
      wait_for_pid_exit "${pid}" 20 || true
      if kill -0 "${pid}" >/dev/null 2>&1; then
        kill "${pid}" >/dev/null 2>&1 || true
        wait_for_pid_exit "${pid}" 20 || true
      fi
      if kill -0 "${pid}" >/dev/null 2>&1; then
        log "Force killing lingering Redis process ${pid}"
        kill -9 "${pid}" >/dev/null 2>&1 || true
        wait_for_pid_exit "${pid}" 10 || true
      fi
    fi
    rm -f "${pid_file}"
  fi
}

write_single_config() {
  local runtime_dir="${RUNTIME_DIR}/single"
  mkdir -p "${runtime_dir}"
  cat >"${runtime_dir}/redis.conf" <<EOF
bind ${SINGLE_HOST}
port ${SINGLE_PORT}
daemonize yes
protected-mode no
save ""
appendonly no
dir ${runtime_dir}
pidfile ${runtime_dir}/redis.pid
logfile ${runtime_dir}/redis.log
EOF
}

single_pid_file() {
  echo "${RUNTIME_DIR}/single/redis.pid"
}

start_single() {
  if [[ "${PROVIDER}" == "docker" ]]; then
    docker_start_single
    return
  fi

  ensure_redis_tools
  stop_single || true
  write_single_config

  log "Starting standalone Redis on ${SINGLE_HOST}:${SINGLE_PORT}"
  "${REDIS_SERVER}" "${RUNTIME_DIR}/single/redis.conf"
  wait_for_redis "${SINGLE_HOST}" "${SINGLE_PORT}" 120 || die "Standalone Redis did not become ready"
}

source_stop_single() {
  shutdown_redis "${SINGLE_HOST}" "${SINGLE_PORT}" "$(single_pid_file)"
}

stop_single() {
  # Best effort across both providers: the resolved provider may have flipped
  # since start (e.g. the Docker daemon went away), and leftover servers hold
  # the same ports either way.
  if [[ "${PROVIDER}" == "docker" ]]; then
    docker_stop_single
    source_stop_single
  else
    source_stop_single
    if docker_available; then
      docker_stop_single
    fi
  fi
}

cluster_node_dir() {
  local index="$1"
  echo "${RUNTIME_DIR}/cluster/node-${index}"
}

cluster_node_port() {
  local index="$1"
  echo "$((CLUSTER_BASE_PORT + index))"
}

write_cluster_config() {
  local index="$1"
  local node_dir
  local node_port
  node_dir="$(cluster_node_dir "${index}")"
  node_port="$(cluster_node_port "${index}")"
  mkdir -p "${node_dir}"

  cat >"${node_dir}/redis.conf" <<EOF
bind ${CLUSTER_HOST}
port ${node_port}
daemonize yes
protected-mode no
save ""
appendonly no
cluster-enabled yes
cluster-config-file nodes.conf
cluster-node-timeout 5000
dir ${node_dir}
pidfile ${node_dir}/redis.pid
logfile ${node_dir}/redis.log
EOF
}

start_cluster_nodes() {
  local index
  ensure_redis_tools

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    write_cluster_config "${index}"
    "${REDIS_SERVER}" "$(cluster_node_dir "${index}")/redis.conf"
  done

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    wait_for_redis "${CLUSTER_HOST}" "$(cluster_node_port "${index}")" 120 ||
      die "Redis Cluster node $(cluster_node_port "${index}") did not become ready"
  done
}

create_cluster() {
  local nodes=()
  local index

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    nodes+=("${CLUSTER_HOST}:$(cluster_node_port "${index}")")
  done

  log "Creating Redis Cluster with ${CLUSTER_NODE_COUNT} nodes"
  "${REDIS_CLI}" --cluster create "${nodes[@]}" --cluster-replicas "${CLUSTER_REPLICAS}" --cluster-yes
}

wait_for_cluster_ok() {
  local index
  local port

  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    port="$(cluster_node_port "${index}")"
    if ! wait_for_redis "${CLUSTER_HOST}" "${port}" 120; then
      return 1
    fi
  done

  for ((index = 0; index < 100; ++index)); do
    if "${REDIS_CLI}" -h "${CLUSTER_HOST}" -p "${CLUSTER_PORT}" cluster info 2>/dev/null | grep -q '^cluster_state:ok'; then
      return 0
    fi
    sleep 0.2
  done

  return 1
}

start_cluster() {
  if [[ "${PROVIDER}" == "docker" ]]; then
    docker_start_cluster
    return
  fi

  [[ "${CLUSTER_NODE_COUNT}" -eq 6 ]] || die "This fixture currently expects 6 cluster nodes"
  [[ "${CLUSTER_REPLICAS}" -eq 1 ]] || die "This fixture currently expects --cluster-replicas 1"

  stop_cluster || true
  start_cluster_nodes
  create_cluster
  wait_for_cluster_ok || die "Redis Cluster did not reach cluster_state:ok"
}

source_stop_cluster() {
  local index
  for ((index = 0; index < CLUSTER_NODE_COUNT; ++index)); do
    shutdown_redis "${CLUSTER_HOST}" "$(cluster_node_port "${index}")" "$(cluster_node_dir "${index}")/redis.pid"
  done
}

stop_cluster() {
  # Best effort across both providers, same rationale as stop_single.
  if [[ "${PROVIDER}" == "docker" ]]; then
    docker_stop_cluster
    source_stop_cluster
  else
    source_stop_cluster
    if docker_available; then
      docker_stop_cluster
    fi
  fi
}

cleanup_runtime() {
  rm -rf "${RUNTIME_DIR}/single" "${RUNTIME_DIR}/cluster"
}

print_env() {
  cat <<EOF
HIREDIS_HAPP_TEST_SINGLE_HOST=${SINGLE_HOST}
HIREDIS_HAPP_TEST_SINGLE_PORT=${SINGLE_PORT}
HIREDIS_HAPP_TEST_CLUSTER_HOST=${CLUSTER_HOST}
HIREDIS_HAPP_TEST_CLUSTER_PORT=${CLUSTER_PORT}
HIREDIS_HAPP_TEST_CLUSTER_BASE_PORT=${CLUSTER_BASE_PORT}
HIREDIS_HAPP_TEST_CLUSTER_NODE_COUNT=${CLUSTER_NODE_COUNT}
HIREDIS_HAPP_TEST_CLUSTER_REPLICAS=${CLUSTER_REPLICAS}
HIREDIS_HAPP_TEST_REDIS_ROOT=${WORK_ROOT}
HIREDIS_HAPP_TEST_REDIS_PROVIDER=${PROVIDER}
EOF
}

redis_cli_ping() {
  local host="$1"
  local port="$2"

  if [[ "${PROVIDER}" == "docker" ]]; then
    if [[ "${port}" == "${SINGLE_PORT}" ]]; then
      # The single container listens on the image default port 6379; the
      # host-side SINGLE_PORT is only the published mapping.
      docker exec "$(docker_container_single)" redis-cli -p 6379 PING >/dev/null 2>&1
    else
      # Cluster containers use host networking, so the announced host and
      # port are reachable from inside the container as well.
      docker exec "$(docker_container_cluster $((port - CLUSTER_BASE_PORT)))" \
        redis-cli -h "${host}" -p "${port}" PING >/dev/null 2>&1
    fi
    return
  fi

  if [[ ! -x "${REDIS_CLI}" ]]; then
    return 1
  fi

  "${REDIS_CLI}" -h "${host}" -p "${port}" PING >/dev/null 2>&1
}

cluster_info_ok() {
  if [[ "${PROVIDER}" == "docker" ]]; then
    docker exec "$(docker_container_cluster 0)" redis-cli -h "${CLUSTER_HOST}" -p "${CLUSTER_PORT}" cluster info 2>/dev/null |
      grep -q '^cluster_state:ok'
    return
  fi

  if [[ ! -x "${REDIS_CLI}" ]]; then
    return 1
  fi

  "${REDIS_CLI}" -h "${CLUSTER_HOST}" -p "${CLUSTER_PORT}" cluster info 2>/dev/null | grep -q '^cluster_state:ok'
}

status() {
  if redis_cli_ping "${SINGLE_HOST}" "${SINGLE_PORT}"; then
    log "single: running on ${SINGLE_HOST}:${SINGLE_PORT} (${PROVIDER})"
  else
    log "single: stopped"
  fi

  if redis_cli_ping "${CLUSTER_HOST}" "${CLUSTER_PORT}"; then
    if cluster_info_ok; then
      log "cluster: running on ${CLUSTER_HOST}:${CLUSTER_PORT} (${PROVIDER})"
    else
      log "cluster: port reachable but cluster_state is not ok"
    fi
  else
    log "cluster: stopped"
  fi
}

main() {
  local command="${1:-}"
  PROVIDER="$(resolve_provider)"
  if [[ "${PROVIDER}" == "homebrew" ]]; then
    # Pre-resolve the Homebrew binary paths so status/stop work without
    # installing anything; start paths install on demand via ensure_redis_tools.
    homebrew_resolve_redis_tools || true
  fi
  case "${command}" in
    download)
      fetch_archive
      ;;
    build)
      ensure_redis_tools
      ;;
    prepare)
      ensure_redis_tools
      ;;
    start-single)
      start_single
      ;;
    stop-single)
      stop_single
      ;;
    restart-single)
      stop_single || true
      start_single
      ;;
    start-cluster)
      start_cluster
      ;;
    stop-cluster)
      stop_cluster
      ;;
    restart-cluster)
      stop_cluster || true
      start_cluster
      ;;
    start-all)
      start_single
      start_cluster
      ;;
    stop-all)
      stop_cluster || true
      stop_single || true
      ;;
    cleanup)
      stop_cluster || true
      stop_single || true
      cleanup_runtime
      ;;
    print-env)
      print_env
      ;;
    status)
      status
      ;;
    ""|-h|--help|help)
      usage
      ;;
    *)
      usage
      die "Unknown command: ${command}"
      ;;
  esac
}

main "$@"
