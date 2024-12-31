# mapapp

Group project for the "Introduction to AI" course. Tested on Windows (MSVC) and Linux (GCC).

All dependencies are bundled in `third_party`.

To build, ensure that all submodules are cloned properly, then run:
```sh
# configure
cmake -S. -Bbuild
# build
cmake --build build
```

To run, export a PBF file from [OpenStreetMap](https://www.openstreetmap.org/export), then pass the path to that file as a command line argument:
```sh
./build/mapapp ~/Downloads/out.osm.pbf
```
