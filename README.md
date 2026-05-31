<h1 align="center"><img src="logo.png"></h1> <br />
BASH-LLVM is as shell built for use in LLM harnesses. We vastly improve the GNU implementation's performance and security, by rewriting the codebase from the ground up as a JIT with LLVM. External programs can be enabled or disabled to prevent an agent from making unwanted changes.


# Building

Requirements:
- **CMake** 3.5 or above
- **A C & C++ compiler** C++23 support needed 
- **Ninja** I use 1.13.2, not sure how specific it needs
- **Tree Sitter CLI**
- **LLVM** version 22 
- **patch** I use GNU patch 2.8

These should be relatively easy to find on Arch Linux and Alpine Linux (edge repos) 

```
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja
sudo ninja install
```
