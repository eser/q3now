#!/bin/bash

"./make-macosx.sh" x86_64

rm -rf ./build/tmp
mkdir ./build/tmp

cp -R ./modfiles/* ./build/tmp/
cp -R ./build/release-darwin-x86_64/baseq3/vm ./build/tmp/
cp ./modfiles/description.txt /Applications/ioquake3/blackmore/

cd ./build/tmp
zip -r /Applications/ioquake3/blackmore/pak0.pk3 -0 . -x "**/.DS_Store"
