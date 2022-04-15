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

## 4. Analyze an example bc file using svf-data-flow executable
```
clang -S -c -g -fno-discard-value-names -emit-llvm example.c -o example.ll
./bin/svf-data-flow example.ll
```

## 5. Analyze a ArduPilot bc file using svf-data-flow executable
- 'trace_target_list.txt' contains a list of configuration parameters.<br>
- This executable reads 'trace_target_list.txt' and then collects all the uses of each configuration parameter.<br>
- You can check the analysis output in 'output.txt'.<br>
- Note that the bc file must be complied by the same LLVM version of this SVF's one (LLVM 13.0.0).<br>
```
./bin/svf-data-flow copter_4_1_llvm_13.bc > output.txt
```
