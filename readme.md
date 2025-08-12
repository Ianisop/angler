## Building
### Linux
use the provided build.sh for linux
```
$ ./build.sh
```
### Windows
> make sure you install MSYS2 with MinGW-64, if you try to use gcc without it, good luck

#### Setup the enviroment
open MSYS2 MINGW64 and get the required packages:
```
$ pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```
clone angler
```
$ git clone https://github.com/Ianisop/angler
```
build the project using the provided build.sh
```
$ cd angler
$ ./build.sh
```

> if you encounter any cache issues after building once, i recommend you do ```$ ./build.sh -clean ```

## Running
run the built binary found in ``` angler/build/bin ```
