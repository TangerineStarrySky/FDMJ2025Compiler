#define DEBUG
#undef DEBUG

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <algorithm>
#include <utility>
#include "treep.hh"
#include "quad.hh"
#include "flowinfo.hh"
#include "quadssa.hh"
#include "temp.hh"

using namespace std;
using namespace quad;

// Forward declarations for internal functions
static void deleteUnreachableBlocks(QuadFuncDecl* func, ControlFlowInfo* domInfo);
static void placePhi(QuadFuncDecl* func, ControlFlowInfo* domInfo);
static void renameVariables(QuadFuncDecl* func, ControlFlowInfo* domInfo);
static void cleanupUnusedPhi(QuadFuncDecl* func);

static void deleteUnreachableBlocks(QuadFuncDecl* func, ControlFlowInfo* domInfo) {
    domInfo->eliminateUnreachableBlocks();
}

// 定义全局变量 A_phi, 记录每个节点需要插入φ函数的变量
map<int, set<int>> A_phi;
vector<int> params_nums;
set<int> is_def;
map<int, tree::Type> temp2type;

static void placePhi(QuadFuncDecl* func, ControlFlowInfo* domInfo) {
    DataFlowInfo* dataFlowInfo = new DataFlowInfo(func);
    dataFlowInfo->findAllVars();
    dataFlowInfo->computeLiveness();

    // 初始化defsites数组，记录每个变量在哪些节点被定义
    map<int, set<int>> defsites;
    for(auto var: dataFlowInfo->allVars){
        for(auto block_stm_pair: (*dataFlowInfo->defs)[var]){
            defsites[var].insert(block_stm_pair.first->entry_label->num);
        }
    }

    // 遍历每个变量
    for(auto a: dataFlowInfo->allVars)
    {
        // 初始化W集合，记录需要插入φ函数的变量, 记录所有可能违反必经节点边界标准的节点
        set<int> W = defsites[a];

        // 当W不为空时，执行循环
        while (!W.empty()) {
            // 从W中移除第一个节点n
            int n = *W.begin();
            W.erase(W.begin());

            // 对于n的每个支配边界点Y
            for (auto Y : domInfo->dominanceFrontiers[n]) {
                // 如果Y不在Aφ[n]中, 且a在该块的liveout-set, 则插入phi
                set<int> s_liveout = (*dataFlowInfo->liveout)[(*domInfo->labelToBlock[Y]->quadlist)[0]];
                if (A_phi[Y].find(a) == A_phi[Y].end() && s_liveout.find(a) != s_liveout.end()) {
                    // 在Y的顶部插入φ函数
                    set<Temp*>* def = new set<Temp*>();
                    set<Temp*>* use = new set<Temp*>();
                    vector<pair<Temp*, Label*>>* args = new vector<pair<Temp*, Label*>>();
                    // 创建新的Temp对象并添加到args中
                    def->insert(new Temp(a));
                    for (auto pred : domInfo->predecessors[Y]) {
                        args->push_back(make_pair(new Temp(a), new Label(pred)));
                    }
                    // 在节点Y中插入新的QuadPhi对象
                    quad::QuadPhi* phiStm = new quad::QuadPhi(nullptr, new tree::TempExp(tree::Type::INT, new Temp(a)), args, def, use);
                    quad::QuadBlock* blockY = domInfo->labelToBlock[Y];
                    blockY->quadlist->insert(blockY->quadlist->begin()+1, phiStm);

                    // 更新Aφ[n]和W
                    A_phi[Y].insert(a);
                    for (auto block_stm_pair: (*dataFlowInfo->defs)[a]) {
                        if(block_stm_pair.first->entry_label->num == Y) {
                            // 如果Y是a的定义节点，则不需要插入φ函数
                            continue;
                        }
                        W.insert(Y);
                    }
                }
            }
        }
    }
}

TempExp* renameTempExp(TempExp* tempExp, map<int, stack<int>>& varStack, map<int, int>& varCount, bool def, set<Temp*>* new_set){
    int i;
    int varNum = tempExp->temp->num;
    temp2type[varNum] = tempExp->type;
    // 如果是参数，则不rename
    if(std::find(params_nums.begin(), params_nums.end(), varNum) != params_nums.end()){
        new_set->insert(tempExp->temp);
        return tempExp;
    }
    // 如果未定义，则不rename
    if(!def && is_def.find(varNum) == is_def.end()){
        new_set->insert(tempExp->temp);
        return tempExp;
    }
    if(!def)
        i = varStack[varNum].top();
    else{
        i = varCount[varNum];
        varCount[varNum]++;
        varStack[varNum].push(i);
    }
    int versionedNum = VersionedTemp::versionedTempNum(varNum, i);
    Temp* newtemp = new Temp(versionedNum);
    new_set->insert(newtemp);
    return new TempExp(tempExp->type, newtemp);
}

QuadTerm* renameQuadTerm(QuadTerm* quadTerm, map<int, stack<int>>& varStack, map<int, int>& varCount, bool def, set<Temp*>* new_set){
    if(quadTerm->kind == QuadTermKind::TEMP){
        int i;
        int varNum = quadTerm->get_temp()->temp->num;
        temp2type[varNum] = quadTerm->get_temp()->type;
        // 如果是参数，则不rename
        if(std::find(params_nums.begin(), params_nums.end(), varNum) != params_nums.end()){
            new_set->insert(quadTerm->get_temp()->temp);
            return quadTerm;
        }
        // 如果未定义，则不rename
        if(!def && is_def.find(varNum) == is_def.end()){
            new_set->insert(quadTerm->get_temp()->temp);
            return quadTerm;
        }
        if(!def)
            i = varStack[varNum].top();
        else{
            i = varCount[varNum];
            varCount[varNum]++;
            varStack[varNum].push(i);
        }
        int versionedNum = VersionedTemp::versionedTempNum(varNum, i);
        Temp* newtemp = new Temp(versionedNum);
        new_set->insert(newtemp);
        TempExp* tempExp = new TempExp(quadTerm->get_temp()->type, newtemp);
        return new QuadTerm(tempExp);
    }
    else return quadTerm;
}

void renameBlockVariables(int block_num, DataFlowInfo* dataFlowInfo, ControlFlowInfo* domInfo, map<int, stack<int>>& varStack, map<int, int>& varCount) {
    #ifdef DEBUG
    cout << "Renaming variables in block " << block_num << endl;
    #endif 
    map<int, stack<int>> copy_varStack = varStack;  // varStack 备份
    set<int> copy_is_def = is_def;          // is_def 备份
    quad::QuadBlock* block = domInfo->labelToBlock[block_num];
    for (auto stm : *block->quadlist) {
        #ifdef DEBUG
        cout << "Renaming stm: " << quadKindToString(stm->kind) << endl;
        #endif
        set<Temp*>* new_def = new set<Temp*>();
        set<Temp*>* new_use = new set<Temp*>();

        // 处理use
        if(stm->kind == QuadKind::MOVE) {
            quad::QuadMove* moveStm = static_cast<quad::QuadMove*>(stm);
            moveStm->src = renameQuadTerm(moveStm->src, varStack, varCount, false, new_use);
        } else if(stm->kind == QuadKind::LOAD) {
            quad::QuadLoad* loadStm = static_cast<quad::QuadLoad*>(stm);
            loadStm->src = renameQuadTerm(loadStm->src, varStack, varCount, false, new_use);
        } else if(stm->kind == QuadKind::STORE) {
            quad::QuadStore* storeStm = static_cast<quad::QuadStore*>(stm);
            storeStm->dst = renameQuadTerm(storeStm->dst, varStack, varCount, false, new_use);
            storeStm->src = renameQuadTerm(storeStm->src, varStack, varCount, false, new_use);
        } else if(stm->kind == QuadKind::CJUMP) {
            quad::QuadCJump* cjumpStm = static_cast<quad::QuadCJump*>(stm);
            cjumpStm->left = renameQuadTerm(cjumpStm->left, varStack, varCount, false, new_use);
            cjumpStm->right = renameQuadTerm(cjumpStm->right, varStack, varCount, false, new_use);
        } else if(stm->kind == QuadKind::RETURN) {
            quad::QuadReturn* returnStm = static_cast<quad::QuadReturn*>(stm);
            returnStm->value = renameQuadTerm(returnStm->value, varStack, varCount, false, new_use);
        } else if(stm->kind == QuadKind::CALL) {
            quad::QuadCall* callStm = static_cast<quad::QuadCall*>(stm);
            callStm->obj_term = renameQuadTerm(callStm->obj_term, varStack, varCount, false, new_use);
            for(int i = 0; i < (*callStm->args).size(); i++) {
                (*callStm->args)[i] = renameQuadTerm((*callStm->args)[i], varStack, varCount, false, new_use);
            }
        } else if(stm->kind == QuadKind::EXTCALL) {
            quad::QuadExtCall* extcallStm = static_cast<quad::QuadExtCall*>(stm);
            for(int i = 0; i < (*extcallStm->args).size(); i++) {
                (*extcallStm->args)[i] = renameQuadTerm((*extcallStm->args)[i], varStack, varCount, false, new_use);
            }
        } else if(stm->kind == QuadKind::MOVE_BINOP){
            quad::QuadMoveBinop* moveBinopStm = static_cast<quad::QuadMoveBinop*>(stm);
            moveBinopStm->left = renameQuadTerm(moveBinopStm->left, varStack, varCount, false, new_use);
            moveBinopStm->right = renameQuadTerm(moveBinopStm->right, varStack, varCount, false, new_use);
        } else if(stm->kind == QuadKind::MOVE_CALL) {
            quad::QuadMoveCall* moveCallStm = static_cast<quad::QuadMoveCall*>(stm);
            quad::QuadCall* callStm = moveCallStm->call;
            callStm->obj_term = renameQuadTerm(callStm->obj_term, varStack, varCount, false, new_use);
            for(int i = 0; i < (*callStm->args).size(); i++) {
                (*callStm->args)[i] = renameQuadTerm((*callStm->args)[i], varStack, varCount, false, new_use);
            }
        }else if(stm->kind == QuadKind::MOVE_EXTCALL) {
            quad::QuadMoveExtCall* moveExtcallStm = static_cast<quad::QuadMoveExtCall*>(stm);
            quad::QuadExtCall* extcallStm = moveExtcallStm->extcall;
            for(int i = 0; i < (*extcallStm->args).size(); i++) {
                (*extcallStm->args)[i] = renameQuadTerm((*extcallStm->args)[i], varStack, varCount, false, new_use);
            }
        }

        // 处理def
        if(stm->kind == QuadKind::MOVE) {
            quad::QuadMove* moveStm = static_cast<quad::QuadMove*>(stm);
            moveStm->dst = renameTempExp(moveStm->dst, varStack, varCount, true, new_def);
        } else if(stm->kind == QuadKind::LOAD) {
            quad::QuadLoad* loadStm = static_cast<quad::QuadLoad*>(stm);
            loadStm->dst = renameTempExp(loadStm->dst, varStack, varCount, true, new_def);
        } else if(stm->kind == QuadKind::MOVE_BINOP){
            quad::QuadMoveBinop* moveBinopStm = static_cast<quad::QuadMoveBinop*>(stm);
            moveBinopStm->dst = renameTempExp(moveBinopStm->dst, varStack, varCount, true, new_def);
        } else if(stm->kind == QuadKind::MOVE_CALL) {
            quad::QuadMoveCall* moveCallStm = static_cast<quad::QuadMoveCall*>(stm);
            moveCallStm->dst = renameTempExp(moveCallStm->dst, varStack, varCount, true, new_def);
        }else if(stm->kind == QuadKind::MOVE_EXTCALL) {
            quad::QuadMoveExtCall* moveExtcallStm = static_cast<quad::QuadMoveExtCall*>(stm);
            moveExtcallStm->dst = renameTempExp(moveExtcallStm->dst, varStack, varCount, true, new_def);
        }else if(stm->kind == QuadKind::PHI) {
            quad::QuadPhi* phiStm = static_cast<quad::QuadPhi*>(stm);
            phiStm->temp = renameTempExp(phiStm->temp, varStack, varCount, true, new_def);
        }

        // Update the statement with the new def and use sets
        for(auto t: *stm->def)
            is_def.insert(t->num);  // 记录是否已定义
        stm->def = new_def;
        if(stm->kind != QuadKind::PHI) stm->use = new_use;
    }

    // 处理phi函数
    for(int Y: domInfo->successors[block_num]){
        set<int> pres = domInfo->predecessors[Y];
        auto it = std::find(pres.begin(), pres.end(), block_num);
        int position = std::distance(pres.begin(), it);
        for(int a: A_phi[Y]){
            if(dataFlowInfo->allVars.find(a) == dataFlowInfo->allVars.end())
                continue;
            // cout << "process phi " << Y << "   var " << a << endl;
            int i = varStack[a].top();
            int versionedNum = VersionedTemp::versionedTempNum(a, i);
            if(is_def.find(a) == is_def.end()) // 如果没定义 不加版本号
                versionedNum = a;
            // cout << "Y:" << Y << "   block: " << block_num << "  version: " << versionedNum << endl;
            quad::QuadBlock* next_block = domInfo->labelToBlock[Y];
            for (auto stm : *next_block->quadlist) {
                if(stm->kind == QuadKind::PHI){
                    quad::QuadPhi* phiStm = static_cast<quad::QuadPhi*>(stm);
                    // cout << VersionedTemp::origTempNum(phiStm->temp->temp->num) << endl;
                    if(phiStm->temp->temp->num == a || VersionedTemp::origTempNum(phiStm->temp->temp->num) == a){
                        (*phiStm->args)[position].first = new Temp(versionedNum);
                        phiStm->use->insert(new Temp(versionedNum));
                        phiStm->temp->type = temp2type[a];  // 补充phi节点的type
                        break;
                    }
                }
            }
        }
    }

    // 处理每一个儿子节点
    for(int child: domInfo->domTree[block_num]) {
        renameBlockVariables(child, dataFlowInfo, domInfo, varStack, varCount);
    }

    // 恢复状态
    varStack = copy_varStack;
    is_def = copy_is_def;

}

static void renameVariables(QuadFuncDecl* func, ControlFlowInfo* domInfo) {
    #ifdef DEBUG
    cout << "Renaming variables for function: " << func->funcname << endl;
    #endif
    temp2type.clear();
    is_def.clear();
    params_nums.clear();
    for(auto t: *func->params){
        params_nums.push_back(t->num);
    }
    DataFlowInfo* dataFlowInfo = new DataFlowInfo(func);
    dataFlowInfo->findAllVars();

    map<int, stack<int>> varStack;  // 记录每个变量的版本号栈
    map<int, int> varCount;  // 记录最新的版本号

    for (auto var : dataFlowInfo->allVars) {
        varCount[var] = 0;
        varStack[var].push(0);
    }
    // 从入口块开始重命名变量
    renameBlockVariables(domInfo->entryBlock, dataFlowInfo, domInfo, varStack, varCount);
}

static void cleanupUnusedPhi(QuadFuncDecl* func) {
    return; // Placeholder for the actual implementation
}

QuadProgram *quad2ssa(QuadProgram* program) {
    // Create a new QuadProgram to hold the SSA version
    QuadProgram* ssaProgram = new QuadProgram(static_cast<tree::Program*>(program->node), new vector<QuadFuncDecl*>());
    // Iterate through each function in the original program
    for (auto func : *program->quadFuncDeclList) {
        // Create a new ControlFlowInfo object for the function
        ControlFlowInfo* domInfo = new ControlFlowInfo(func);
        // Compute control flow information
        domInfo->computeEverything();
        
        // Eliminate unreachable blocks
        deleteUnreachableBlocks(func, domInfo);
        
        // Place phi functions
        placePhi(func, domInfo);
        
        // Rename variables
        renameVariables(func, domInfo);
        
        // Cleanup unused phi functions
        cleanupUnusedPhi(func);
        
        // Add the SSA version of the function to the new program
        ssaProgram->quadFuncDeclList->push_back(func);
    }
    return ssaProgram; //uncomment this line
}
