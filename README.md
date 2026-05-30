<h1 align="center"><img src="logo.png"></h1> <br />
`bash-llvm` is a AI first shell. Vastly improving the GNU implementation's performance and security, by rewriting the codebase from the ground up as a JIT.


# Building

Requirements:
- **CMake** 3.5 or above
- **A C & C++ compiler** C++23 support needed 
- **Ninja** I use 1.13.2, not sure how specific it needs
- **Tree Sitter CLI** 22 or above
- **patch** I use GNU patch 2.8

```
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja
sudo ninja install
```
