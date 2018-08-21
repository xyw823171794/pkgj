#!/bin/bash

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <version>"
  exit 1
fi

version=$1

echo -n $version > version

rm pkgj-v*.vpk
cp ci/build/pkgj.vpk pkgj-v$version.vpk

git add -u
git add version pkgj-v$version.vpk
git commit -m "Push v$version"
git push origin last
