//===- svf-data-flow.cpp -- Data flow analysis by using SVF-------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 // A driver program of SVF including usages of SVF APIs
 //
 // Author: Yulei Sui,
 //         Hyungsub Kim (I adapted 'svf-ex.cpp' to implement data-flow analysis)
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <llvm/Support/CommandLine.h>
#include <llvm/IR/ValueSymbolTable.h>

#include "SVF-FE/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "WPA/Andersen.h"
#include "SVF-FE/SVFIRBuilder.h"
#include <SVF-FE/PAGBuilder.h>
#include "Util/Options.h"

using namespace llvm;
using namespace std;
using namespace SVF;

#define PROGRESS_PRINT 1
#define PRINT_CMD 0
#define LOAD_TRACE 0
#define POINTER_TRACE 1		// You need to turn on 'POINTER_TRACE' if you want to analyze data flow of configuration parameters,
#define TRACE_ON 1
#define TARGET_OR_RANGE 10	// 10: Trace on a specific variable, 20: Trace on all variable within specific code range
#define TRACE_MAX 1000

bool trace_start = false;
std::string TraceTarget[TRACE_MAX];
unsigned int trace_cnt;

// Trace target variable
//std::string trace_target_str = "_terrain_alt";
std::string trace_target_str = "target_code_snippet_1";

static llvm::cl::opt<std::string> InputFilename(cl::Positional,
        llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

// An example to query alias results of two LLVM values
SVF::AliasResult aliasQuery(PointerAnalysis* pta, Value* v1, Value* v2)
{
    return pta->alias(v1,v2);
}

// An example to print points-to set of an LLVM value
std::string printPts(PointerAnalysis* pta, Value* val)
{

    std::string str;
    raw_string_ostream rawstr(str);

    NodeID pNodeId = pta->getPAG()->getValueNode(val);
    const PointsTo& pts = pta->getPts(pNodeId);
    for (PointsTo::iterator ii = pts.begin(), ie = pts.end();
            ii != ie; ii++)
    {
        rawstr << " " << *ii << " ";
        PAGNode* targetObj = pta->getPAG()->getGNode(*ii);
        if(targetObj->hasValue())
        {
            rawstr << "(" <<*targetObj->getValue() << ")\t ";
        }
    }

    return rawstr.str();

}

// An example to query/collect all successor nodes from a ICFGNode (iNode) along control-flow graph (ICFG)
void traverseOnICFG(ICFG* icfg, const Instruction* inst)
{
    ICFGNode* iNode = icfg->getICFGNode(inst);
    FIFOWorkList<const ICFGNode*> worklist;
    Set<const ICFGNode*> visited;
    worklist.push(iNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const ICFGNode* iNode = worklist.pop();
        for (ICFGNode::const_iterator it = iNode->OutEdgeBegin(), eit =
                    iNode->OutEdgeEnd(); it != eit; ++it)
        {
            ICFGEdge* edge = *it;
            ICFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }
}

// An example to query/collect all the uses of a definition of a value along value-flow graph (VFG)
void traverseOnVFG(const SVFG* vfg, Value* val)
{
    SVFIR* pag = SVFIR::getPAG();
    PAGNode* pNode = pag->getGNode(pag->getValueNode(val));

    //outs() << "[pNode]" << *pNode << "\n";


   	const VFGNode* vNode = vfg->getDefSVFGNode(pNode);
   	//const VFGNode* vNode = vfg->fromValue(val);

    //outs() << "[traverseOnVFG] getDefSVFGNode() called." << "\n";

    FIFOWorkList<const VFGNode*> worklist;
    Set<const VFGNode*> visited;
    worklist.push(vNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const VFGNode* vNode = worklist.pop();
        for (VFGNode::const_iterator it = vNode->OutEdgeBegin(), eit =
                    vNode->OutEdgeEnd(); it != eit; ++it)
        {
            VFGEdge* edge = *it;
            VFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }

    /// Collect all LLVM Values
    for(Set<const VFGNode*>::const_iterator it = visited.begin(), eit = visited.end(); it!=eit; ++it)
    {
        const VFGNode* node = *it;
        if(node->getValue() != 0x0) {
        	if (TRACE_ON) {
        		//outs() << "[Collect all LLVM Values]" << node->getValue() << " (" << *(node->getValue()) << ")" << "\n";

        		// We need to extract only 'store' and 'load' instruction.
        		if (isa<StoreInst>(node->getValue())) {
        			auto *store = dyn_cast<StoreInst>(node->getValue());

        			Value* store_operand = (Value*) store->getPointerOperand()->stripPointerCasts();
        			Value* store_operand_2 = (Value*) store->getValueOperand()->stripPointerCasts();

        			std::string operand_1_str = (std::string) store_operand->getName();
        			//std::string operand_2_str = (std::string) store_operand_2->getName();

        			//outs() << "[Trace] " << operand_1_str << ", "<< operand_2_str << "\n";
        			if (operand_1_str.empty() == 0) {
        				//outs() << "[Trace] " << operand_1_str << "\n";

        				const DebugLoc &location = store->getDebugLoc();

        				// Print variable, file name, line number if there is debug info
        				if (location) {
        					int line = location.getLine();
        					const DILocation *debug_info =location.get();

        					outs() << "[Trace] " << operand_1_str << ", " << debug_info->getFilename() << ", " << line << "\n";
        				}
        				// Print only variable if there is no debug info
        				else {
        					outs() << "[Trace] " << operand_1_str << "\n";
        				}


        			}
        		}
        	}
        }
    }

    if (PRINT_CMD) {
    	outs() << "-------------------------------- \n\n";
    }
}

std::string trim_string(std::string str) {

	for(int index = 0; index< str.length(); index++){
		if(isdigit(str[index])){
			str.erase(index, 1);
			index--;
		}
	}

	return str;
}

// Return 'true' if current operand is matched with a target variable
bool is_target(std::string str) {

	bool found = false;
	for(int index = 0; index < trace_cnt; index++) {
		if( TraceTarget[index].compare(str) == 0 ) {
			found = true;
			//cout << "[DEBUG] Found target variable: " << str << "\n";
		}
	}

	return found;
}

int main(int argc, char ** argv)
{

    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    SVFUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Whole Program Points-to Analysis\n");
    // Read from the text file
    ifstream trace_target("trace_target_list.txt");

	if (TARGET_OR_RANGE == 10) {
		if (trace_target.is_open()) {
			// Use a while loop together with the getline() function to read the file line by line
			string line;
			while (getline (trace_target, line)) {
				// Output the text from the file
				cout << "[TRACE_TARGET] " << trace_cnt << ". " << line << "\n";
				TraceTarget[trace_cnt] = line;
				trace_cnt++;
			}
		}
		else {
			cout << "Unable to open the trace target file.\n";
		}
	}

    if (Options::WriteAnder == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    Module* llvmModule = SVF::LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();

    svfModule->buildSymbolTableInfo();

    /// Build Program Assignment Graph (SVFIR)
    SVFIRBuilder builder;
    SVFIR* pag = builder.build(svfModule);
    if (PROGRESS_PRINT) {
    	outs() << "[PROGRESS] Finished to Build Program Assignment Graph (SVFIR)\n";
    }

    /// Create Andersen's pointer analysis
    Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    if (PROGRESS_PRINT) {
    	outs() << "[PROGRESS] Finished to Create Andersen's pointer analysis\n";
    }
    /// Query aliases
    /// aliasQuery(ander,value1,value2);

    /// Print points-to information
    /// printPts(ander, value1);

    /// Call Graph
    PTACallGraph* callgraph = ander->getPTACallGraph();
    if (PROGRESS_PRINT) {
    	outs() << "[PROGRESS] Finished to Create Call Graph\n";
    }

    /// ICFG
    //ICFG* icfg = pag->getICFG();

    /// Value-Flow Graph (VFG)
    //VFG* vfg = new VFG(callgraph);
    //outs() << "[Finished to Create Value-Flow Graph (VFG)]\n";

    /// Sparse value-flow graph (SVFG)
    SVFGBuilder svfBuilder;
    SVFG* svfg = svfBuilder.buildFullSVFG(ander);
    if (PROGRESS_PRINT) {
    	outs() << "[PROGRESS] Finished to Create Sparse value-flow graph (SVFG)\n";
    }

    // Store SVFG into *.dot format
    //svfg->dump("svfg");

    if (TRACE_ON) {
		for (const auto& [_, node] : *svfg) {
			//outs() << "[Node] " << *node << "\n";
			if (auto s_node = llvm::dyn_cast<SVF::StmtVFGNode>(node)) {

				const SVF::PAGEdge* edge = s_node->getPAGEdge();

				if (PRINT_CMD) {
					outs() << "---------------[StmtVFGNode]----------------- \n";
					cout << "[Corresponding PAGEdge] " << *edge << "\n";
				}

				const llvm::Value* val = NULL;
				val = edge->getValue();

				if (val != NULL){

					llvm::Value* v = (llvm::Value*) val;

					// Handle store instruction
					if (isa<StoreInst>(val)) {
						auto *store = dyn_cast<StoreInst>(val);

						if (PRINT_CMD) {
							outs() << "[dyn_cast<StoreInst>]: " << store->getPointerOperand()->stripPointerCasts() << " | " << store->getValueOperand()->stripPointerCasts() << "\n";
							outs() << "[dyn_cast<StoreInst>]: " << *(store->getPointerOperand()->stripPointerCasts()) << " | " << *(store->getValueOperand()->stripPointerCasts()) << "\n";
						}
						Value* store_operand = (Value*) store->getPointerOperand()->stripPointerCasts();
						Value* store_operand_2 = (Value*) store->getValueOperand()->stripPointerCasts();

						std::string operand_1_str = (std::string) store_operand->getName();
						std::string operand_2_str = (std::string) store_operand_2->getName();

						operand_1_str = trim_string(operand_1_str);
						operand_2_str = trim_string(operand_2_str);

						if (PRINT_CMD) {
							outs() << "[variable_name] " << operand_1_str << "\n";
							outs() << "[variable_name] " << operand_2_str << "\n";
						}

						if (TARGET_OR_RANGE == 10) {
							bool time_to_trace = false;
							time_to_trace = is_target(operand_1_str);

							if( time_to_trace) {
								outs() << "[traverseOnVFG] Let's trace: " << operand_1_str << ", " << operand_2_str << "\n";
								traverseOnVFG(svfg, store_operand);
							}
						}
						// Trace on all variable within specific code range
						else if (TARGET_OR_RANGE == 20) {
							if( trace_target_str.compare(operand_1_str) == 0) {
								if (trace_start == false) {
									trace_start = true;
								}
								else {
									trace_start = false;
								}
								continue;
							}

							if (trace_start) {
								outs() << "[traverseOnVFG] Let's trace: " << operand_1_str << ", " << operand_2_str << "\n";
								traverseOnVFG(svfg, store_operand);
							}
						}
					}

					// Handle load instruction
					if ( LOAD_TRACE && isa<LoadInst>(val) ) {
						auto *load = dyn_cast<LoadInst>(val);
						if (PRINT_CMD) {
							outs() << "[dyn_cast<LoadInst>]: " << load->getPointerOperand() << "\n";
							outs() << "[dyn_cast<LoadInst>]: " << *(load->getPointerOperand()) << "\n";
						}

						Value* load_operand = (Value*) load->getPointerOperand();
						std::string operand_str = (std::string) load_operand->getName();

						operand_str = trim_string(operand_str);

						if (PRINT_CMD) {
							outs() << "[variable_name] " << operand_str << "\n";
						}

						bool time_to_trace = false;
						time_to_trace = is_target(operand_str);

						if(time_to_trace) {
							outs() << "[traverseOnVFG] Let's trace: " << operand_str << "\n";
							traverseOnVFG(svfg, load_operand);
						}
					}

					// Handle pointer instruction
					if ( POINTER_TRACE && isa<GetElementPtrInst>(val) ) {
						auto gep = dyn_cast<GetElementPtrInst>(val);
						if (PRINT_CMD) {
							outs() << "[dyn_cast<GetElementPtrInst>]: " << gep->getName() << "\n";
							outs() << "[dyn_cast<GetElementPtrInst>]: " << gep->getPointerOperand() << "\n";
						}

						std::string operand_str = (std::string) gep->getName();

						operand_str = trim_string(operand_str);

						if (PRINT_CMD) {
							outs() << "[variable_name] " << operand_str << "\n";
						}
						bool time_to_trace = false;
						time_to_trace = is_target(operand_str);

						if(time_to_trace) {
							outs() << "[traverseOnVFG] Let's trace: " << operand_str << "\n";

							Value *pointer_operand = (Value *) gep->getPointerOperand();
							traverseOnVFG(svfg, pointer_operand);
						}
					}
				}
			}
			// [Function] Actual Parameter
			else if (auto s_node = llvm::dyn_cast<SVF::ActualParmVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[ActualParmVFGNode]----------------- \n";
				}

			}
			// [Function] Formal Parameter
			else if (auto s_node = llvm::dyn_cast<SVF::FormalParmVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[FormalParmVFGNode]----------------- \n";
				}

			}
			// [Function] CallSite Return Variable
			else if (auto s_node = llvm::dyn_cast<SVF::ActualRetVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[ActualRetVFGNode]----------------- \n";
				}

			}
			// [Function] Procedure Return Variable
			else if (auto s_node = llvm::dyn_cast<SVF::FormalRetVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[FormalRetVFGNode]----------------- \n";
				}

			}
			else if (auto s_node = llvm::dyn_cast<SVF::BinaryOPVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[BinaryOPVFGNode]----------------- \n";
				}

			}
			else if (auto s_node = llvm::dyn_cast<SVF::CmpVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[CmpVFGNode]----------------- \n";
				}

			}
			else if (auto s_node = llvm::dyn_cast<SVF::UnaryOPVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[UnaryOPVFGNode]----------------- \n";
				}

			}
			else if (auto s_node = llvm::dyn_cast<SVF::PHIVFGNode>(node)) {
				if (PRINT_CMD) {
					outs() << "---------------[PHIVFGNode]----------------- \n";
				}

			}
		}
    }

    /// Collect uses of an LLVM Value
    //traverseOnVFG(svfg, store_test);

    /// Collect all successor nodes on ICFG
    /// traverseOnICFG(icfg, value);

    // clean up memory
    //delete vfg;
    delete svfg;
    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();

    // Close the file
    trace_target.close();

    //LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
    llvm::llvm_shutdown();

    return 0;
}
