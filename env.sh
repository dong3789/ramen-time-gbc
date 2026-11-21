# source this:  source ./env.sh
export GBDKDIR="${GBDKDIR:-$HOME/gbdk/}"
# trailing slash required by lcc
case "$GBDKDIR" in */) ;; *) GBDKDIR="$GBDKDIR/" ;; esac
export GBDKDIR
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -x "$HERE/.venv/bin/python3" ]; then
  export PATH="$HERE/.venv/bin:$PATH"
fi
