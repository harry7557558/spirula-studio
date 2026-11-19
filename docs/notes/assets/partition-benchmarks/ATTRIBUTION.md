# London comparison images

`london-ownership.png` contains resized ground-truth photographs from the
Zip-NeRF London dataset, credited to Jonathan T. Barron, Ben Mildenhall,
Dor Verbin, Pratul P. Srinivasan and Peter Hedman (2023).
The [Zip-NeRF project](https://jonbarron.info/zipnerf/) links to the
[SMERF data release](https://smerf-3d.github.io/), which releases these datasets
under [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).

The other columns are Spirula reconstructions of the same capture. Images were
resized and composed with metric labels; rows select the largest, median and
smallest Voronoi-minus-ViewGraph PSNR differences across 248 held-out views.
The comparison image is distributed under CC BY 4.0.

`london-camera-partitions.png` plots the same dataset's 1,984 camera centers
projected onto two principal axes, colored by each strategy's core camera
assignment. It is also distributed under CC BY 4.0 with the dataset credit above.
