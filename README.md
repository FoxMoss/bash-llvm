<h1 align="center"><img src="logo.png"></h1> <br />
BASH-LLVM is as shell built for use in LLM harnesses. We vastly improve the GNU implementation's performance and security, by rewriting the codebase from the ground up as a LLVM frontend. BASH-LLVM has an about a 20x runtime speed improvement over GNU BASH even at this early point in development.

# Building

Requirements:
- **CMake** 3.5 or above
- **A C & C++ compiler** C++23 support needed 
- **Ninja or Make** I use Ninja 1.13.2, and the instructions will assume you have ninja
- **patch** I use GNU patch 2.8
- **git**

These should be relatively easy to find on Arch Linux and Alpine Linux (edge repos) 

```
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja
sudo ninja install
```
