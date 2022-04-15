## Intro
We adapted https://github.com/SVF-tools/SVF-example to implement data flow analysis in drone control software (e.g., ArduPilot and PX4). <br>
We release this source code in the hope of benefiting others.<br>
You are kindly asked to acknowledge usage of the tool by citing SVF papers (http://svf-tools.github.io/SVF) as well as our paper.<br>
```
@inproceedings{kim2021pgfuzz,
  title={PGFUZZ: Policy-Guided Fuzzing for Robotic Vehicles},
  author={Kim, Hyungsub and Ozmen, Muslum Ozgur and Bianchi, Antonio and Celik, Z Berkay and Xu, Dongyan},
  booktitle={Proceedings of the Network and Distributed System Security Symposium (NDSS)},
  year={2021}
}
```

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

## 5. Analyze an ArduPilot bc file using svf-data-flow executable
- 'trace_target_list.txt' contains a list of configuration parameters.<br>
- This executable reads 'trace_target_list.txt' and then collects all the uses of each configuration parameter.<br>
- You can check the analysis output in 'output.txt'.<br>
- Note that the bc file must be complied by the same LLVM version of this SVF's one (LLVM 13.0.0).<br>
```
./bin/svf-data-flow copter_4_1_llvm_13.bc > output.txt
```
## 6. Tips 
- The bc file must be complied by the same LLVM version of this SVF's one (LLVM 13.0.0).<br>
- If you want to analyze other configuration parameters, please put new configuration parameters into 'trace_target_list.txt'<br>
- This data flow analysis also can be used to trace other variables in drone control software.<br>
