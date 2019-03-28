#!/bin/bash 
mkdir -p logo.iconset
cd logo.iconset
convert -resize 16x16 ../logo.png icon_16x16.png
convert -resize 32x32 ../logo.png icon_16x16@2x.png
convert -resize 32x32 ../logo.png icon_32x32.png
convert -resize 64x64 ../logo.png icon_32x32@2x.png
convert -resize 128x128 ../logo.png icon_128x128.png
convert -resize 256x256 ../logo.png icon_128x128@2x.png
convert -resize 256x256 ../logo.png icon_256x256.png
convert -resize 512x512 ../logo.png icon_256x256@2x.png
convert -resize 512x512 ../logo.png icon_512x512.png
convert -resize 1024x1024 ../logo.png icon_512x512@2x.png
cd ..
iconutil -c icns logo.iconset
cp logo.icns ../logo.icns
