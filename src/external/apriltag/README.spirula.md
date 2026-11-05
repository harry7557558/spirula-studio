AprilTag is vendored from https://github.com/AprilRobotics/apriltag .
The ArUco 4x4, 5x5, 6x6 and 7x7 tables are unmodified files from
`b7c0ebe9aa20f82ec7a828579004f9e706bfecd9` (`aruco/`). Their first 50, 100 and 250
entries also cover those smaller OpenCV dictionaries, with the same IDs.

The detector is BSD-2-Clause (`LICENSE.md`). The ArUco tables additionally
carry OpenCV's Apache-2.0 notice (`LICENSE-OpenCV.txt`). No OpenCV runtime
or build dependency is used.
