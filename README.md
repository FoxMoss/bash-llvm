![](logo.png)

`bash-llvm` is Bash implementation built for modern age. Built from the ground up with security and performance in mind.

`bash-llvm` has an about a 20x runtime speed improvement over GNU Bash, with a 4x improvement as a JIT.

# Installing

If you would like the quick easy way out to install BASH-LLVM on Linux, run the installer
```sh
bash -c "$(COLUMNS=50 curl https://raw.githubusercontent.com/FoxMoss/llsh-installer/refs/heads/main/install.sh -#)"
```

Code is fully auditable at [llsh-installer](https://github.com/FoxMoss/llsh-installer).

# Building

Requirements:
- **CMake** 3.5 or above
- **A C & C++ compiler** C++23 support needed 
- **Ninja** or make if you know how to use it
- **patch**
- **git**

All other dependencies are handled by CMake and CPM, and fetched in at build time.

These should be relatively easy to find on Arch Linux and Alpine Linux (edge repos) 

```sh
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja
sudo ninja install
```
