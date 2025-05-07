# DYI submission
set -e

git add .

MSG="${1:-test}"
git commit -m "$MSG"

git push