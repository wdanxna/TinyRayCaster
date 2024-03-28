ffmpeg -f image2 -i frame_%03d.png -vf "palettegen" -y palette.png
ffmpeg -f image2 -i frame_%03d.png -i palette.png -lavfi "paletteuse" -y output.gif