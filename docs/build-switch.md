# Build Switch skeleton

Prerequisite: installed devkitPro with devkitA64, libnx and Switch SDL2 portlibs. From `port/switch` run `make -f Makefile` in the devkitPro MSYS shell. Output: `SpaceRangersHDAWarApart.nro`.

The executable contains no game asset. On device, a user-supplied installed-game copy is expected under `sdmc:/switch/space-rangers-hd-a-war-apart/game`; `DATA/common.pkg` is parsed through the first package-loader stage (root offset, root-folder metadata, fixed 158-byte record) and its metadata is processed by the translated `CrcUnit`. Log path: `sdmc:/switch/space-rangers-hd-a-war-apart/port.log`.

Host build PASS is not hardware proof.
