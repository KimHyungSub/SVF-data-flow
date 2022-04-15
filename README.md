## 1. Install SVF and its dependence (LLVM pre-built binary) via npm
```
npm i --silent svf-lib --prefix ${HOME}
```

## 2. Clone repository
```
git clone https://github.com/KimHyungSub/SVF-data-flow.git
```

## 3. Setup SVF environment and build your project 
```
source ./env.sh
```
cmake the project (`cmake -DCMAKE_BUILD_TYPE=Debug .` for debug build)
```
cmake . && make
```

## 4. Analyze a example bc file using svf-data-flow executable
```
clang -S -c -g -fno-discard-value-names -emit-llvm example.c -o example.ll
./bin/svf-data-flow example.ll
```

## 5. Analyze a ArduPilot bc file using svf-data-flow executable
```
./bin/svf-data-flow copter_4_1_llvm_13.bc > output.txt
```
