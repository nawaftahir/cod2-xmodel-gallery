#!/usr/bin/env bash
# Build the full model gallery and commit it to a local `gh-pages` branch, ready to
# push. Source stays on `main`; the gallery (index.html + thumbnails) lives on
# `gh-pages`. Does NOT push — it prints the push commands for you to run.
#
#   ./deploy.sh --basepath="/path/to/Call of Duty 2"
#   ./deploy.sh --loose=/path/to/stockrawfiles
set -euo pipefail
cd "$(dirname "$0")"
REPO="$(pwd)"
BIN=build/cod2-xmodel-gallery
[ -x "$BIN" ] || ./build.sh

OUT="$REPO/gallery"
rm -rf "$OUT"; mkdir -p "$OUT"
"$BIN" "$@" --batch --outdir="$OUT" --width=480 --height=360 --quality=88

# Publish OUT/ onto an orphan gh-pages branch via a throwaway worktree so the main
# working tree (source) is never touched and .gitignore can't drop the thumbnails.
WT="$(mktemp -d)"
git worktree add --quiet --detach "$WT"
(
  cd "$WT"
  git checkout --quiet --orphan gh-pages
  git rm -rfq . >/dev/null 2>&1 || true
  cp -r "$OUT"/. .
  touch .nojekyll
  git add -A
  git -c user.name="Nawaf Tahir" -c user.email="143700542+nawaftahir@users.noreply.github.com" \
      commit -q -m "Publish model gallery"
)
git worktree remove --force "$WT"

echo
echo "gh-pages branch built ($(du -sh "$OUT" | cut -f1)). To publish:"
echo "  git remote add origin <your GitHub repo URL>   # if not already set"
echo "  git push -u origin main"
echo "  git push -u origin gh-pages"
echo "  # then in the repo: Settings -> Pages -> Branch: gh-pages / root"
