#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Qualify the installed Debian product, including Debian's compressed docs.
set -euo pipefail
: "${RUNNER_TEMP:?}"
expected_version=${1:?Expected package version is required}
test "$(runnerscope --version)" = "$expected_version"
runnerscope --self-test
XDG_CONFIG_HOME="$RUNNER_TEMP/runner-ui-config" \
XDG_STATE_HOME="$RUNNER_TEMP/runner-ui-state" \
  xvfb-run -a runnerscope --ui-self-test
executable_type=$(file /usr/bin/runnerscope)
grep -q ELF <<< "$executable_type"
runtime_libraries=$(ldd /usr/bin/runnerscope)
if grep -Eiq 'python|libtk|tkinter' <<< "$runtime_libraries"; then
  echo 'Unexpected interpreter or Tk runtime linkage.' >&2
  exit 1
fi
test -s /usr/share/doc/runnerscope/COPYING || test -s /usr/share/doc/runnerscope/COPYING.gz
if test -s /usr/share/doc/runnerscope/COPYING.gz; then
  gzip -t /usr/share/doc/runnerscope/COPYING.gz
fi
test -s /usr/share/doc/runnerscope/cJSON-LICENSE
test -s /usr/share/doc/runnerscope/GitHub-CLI-LICENSE
test -f /usr/share/applications/runnerscope.desktop
test -f /usr/share/icons/hicolor/scalable/apps/runnerscope.svg
test -f /usr/share/icons/hicolor/128x128/apps/runnerscope.png
test -f /usr/share/fonts/truetype/runnerscope/mb_corpo_a_cond_regular.ttf
test -f /usr/share/fonts/truetype/runnerscope/mb_corpo_s_bold.ttf
test -f /usr/share/fonts/truetype/runnerscope/mb_corpo_s_regular.ttf
for asset in nav-runners nav-active nav-history nav-health nav-infiltrator \
             hero-runners hero-active hero-history hero-health; do
  test -s "/usr/share/runnerscope/art/${asset}.png"
  gdk-pixbuf-thumbnailer -s 64 "/usr/share/runnerscope/art/${asset}.png" \
    "$RUNNER_TEMP/${asset}.png"
done
ui_font=$(fc-match "MB Corpo S Title WEB")
brand_font=$(fc-match "MB Corpo A Title Cond WEB")
grep -qi 'MB Corpo' <<< "$ui_font"
grep -qi 'MB Corpo' <<< "$brand_font"
grep -qx 'Exec=/usr/bin/runnerscope' /usr/share/applications/runnerscope.desktop
test ! -e /usr/lib/runnerscope/runnerscope.py
