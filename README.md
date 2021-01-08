# sge-server

### Introduction
A simple tcp server.

#### build
Check if python3 is installed and python3-config is in the `$PATH`
```bash
mkdir build && cd build
cmake ..
```

#### use
1. First modify this configuration file example/config.py
```bash
cd build
cp -r ../src/python-lib .
./sge-server ../example/config.py
```
