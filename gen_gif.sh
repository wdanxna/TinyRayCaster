ffmpeg -f image2 -i frame_%03d.ppm -vf "palettegen" -y palette.png
ffmpeg -f image2 -i frame_%03d.ppm -i palette.png -lavfi "paletteuse" -y output.gif