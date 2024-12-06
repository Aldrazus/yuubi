# Yuubi

A toy rendering engine built with Vulkan and C++23.

## Setup

### Cloning the repo

```sh
git clone https://github.com/aldrazus/yuubi
git submodule update --init --recursive
```

### Prerequisites

- CMake 3.26

### Building

Generate the project

```pwsh
cmake -B build -S .
```

Build the project in debug mode

```pwsh
cmake --build build --config Debug 
```

OR build the project in release mode

```pwsh
cmake --build build --config Release
```

### Running

```pwsh
./build/<Debug or Release>/yuubi
```

## Project Structure

- `external`: third-party libraries
- `src`: source code
