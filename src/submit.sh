# DYI submission
set -e

git add .

MSG="${1:-test}"
git commit --allow-empty -m "$MSG"

git push origin master