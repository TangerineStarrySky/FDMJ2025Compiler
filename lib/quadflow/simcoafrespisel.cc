#define DEBUG
#undef DEBUG

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "temp.hh"
#include "ig.hh"
#include "coloring.hh"

bool isAnEdge(map<int, set<int>>& graph, int src, int dst) {
    if (graph.find(src) == graph.end()) return false; //src not in the graph
    if (graph[src].find(dst) == graph[src].end()) return false; //dst not in the graph
    return true; //edge exists
}

//return true if any node is removed
bool Coloring::simplify() {
    #ifdef DEBUG
    cout << "enter simplify  ";
    #endif
    bool changed = false;
    
    // Find all nodes with degree < k that aren't machine registers
    vector<int> to_remove;
    for (const auto& [node, neighbors] : graph) {
        if (!isMachineReg(node) && neighbors.size() < k && !isMove(node)) {
            to_remove.push_back(node);
        }
    }
    
    // Remove these nodes from the graph and push to stack
    for (int node : to_remove) {
        simplifiedNodes.push(node);
        eraseNode(node);
        changed = true;
    }

    #ifdef DEBUG
    cout << "simplified: ";
    for (int node : to_remove) {
        cout << node << " ";
    }
    cout << endl;
    #endif
    
    return changed;
}

//return true if changed anything, false otherwise
bool Coloring::coalesce() {
    #ifdef DEBUG
    cout << "enter coalesce  ";
    #endif
    bool changed = false;
    
    for (auto it = movePairs.begin(); it != movePairs.end(); ++it) {
        int u = it->first;
        int v = it->second;

        if(isAnEdge(graph, u, v)) continue;
        
        // 确定主导节点(lead node)
        int lead, other;
        
        // 规则1：机器寄存器必须作为主导节点
        if (isMachineReg(u)) {
            lead = u;
            other = v;
        } else if (isMachineReg(v)) {
            lead = v;
            other = u;
        } 
        // 规则2：都不是机器寄存器时，选择度数较大的作为主导节点
        else {
            lead = (graph[u].size() >= graph[v].size()) ? u : v;
            other = (lead == u) ? v : u;
        }
        
        // 检查合并后是否满足Briggs准则
        set<int> combined_neighbors;
        if (graph.count(lead)) combined_neighbors.insert(graph[lead].begin(), graph[lead].end());
        if (graph.count(other)) combined_neighbors.insert(graph[other].begin(), graph[other].end());
        
        int high_degree_count = 0;
        int machine_reg_count = 0;
        for (int n : combined_neighbors) {
            if (isMachineReg(n)){
                machine_reg_count++;
            }
            if (graph[n].size() >= k) {
                high_degree_count++;
                // cout << "high_degree node: " << n << endl;
                if (high_degree_count >= k) 
                    break;
            }
        }
        // cout << "combined_neighbors: ";
        // for (int n : combined_neighbors) {
        //     cout << n << " ";
        // }
        
        if (high_degree_count < k && machine_reg_count < k) {
            // 执行合并操作
            if (graph.count(other)) {
                for (int neighbor : graph[other]) {
                    addEdge(lead, neighbor);
                }
                eraseNode(other);
            }
            
            // 记录合并关系（支持递归合并）
            coalescedMoves[lead].insert(other);
            if (coalescedMoves.count(other)) {
                coalescedMoves[lead].insert(coalescedMoves[other].begin(), 
                                           coalescedMoves[other].end());
                coalescedMoves.erase(other);
            }
            
            // 更新移动指令对
            it = movePairs.erase(it);
            changed = true;

            // 更新其他涉及被合并节点的移动对
            for (auto it2 = movePairs.begin(); it2 != movePairs.end(); ) {
                if (it2->first == other || it2->second == other) {
                    // 创建新的移动对，用主导节点替换被合并节点
                    int new_u = (it2->first == other) ? lead : it2->first;
                    int new_v = (it2->second == other) ? lead : it2->second;
                    
                    // 避免自移动(lead <-> lead)
                    if (new_u != new_v) {
                        movePairs.insert({new_u, new_v});
                    }
                    it2 = movePairs.erase(it2);
                } else {
                    ++it2;
                }
            }
            
            #ifdef DEBUG
            cout << "Coalesced: " << other << " into " << lead << endl;
            #endif

            break;
        } 
    }
    
    return changed;
}

//freeze the moves that are not coalesced
//return true if changed anything, false otherwise
bool Coloring::freeze() {
    #ifdef DEBUG
    cout << "enter freeze  ";
    #endif
    bool changed = false;
    
    // 查找可以冻结的移动指令（两个节点的度数都 < k）
    for (auto it = movePairs.begin(); it != movePairs.end(); it++) {
        int u = it->first;
        int v = it->second;
        
        bool u_in_graph = graph.count(u);
        bool v_in_graph = graph.count(v);
        
        if ((!u_in_graph || graph[u].size() < k) && 
            (!v_in_graph || graph[v].size() < k)) {
            
            // 冻结这条移动指令
            it = movePairs.erase(it);
            changed = true;
            
            #ifdef DEBUG
            cout << "Frozen move: " << u << " <-> " << v << endl;
            #endif
            
            // 冻结后可能允许继续简化
            if (u_in_graph && graph[u].size() < k && !isMove(u)) {
                simplifiedNodes.push(u);
                eraseNode(u);
            }
            if (v_in_graph && graph[v].size() < k && !isMove(v)) {
                simplifiedNodes.push(v);
                eraseNode(v);
            }
            break;
        }
    }
    
    return changed;
}

//This is a soft spill: we just remove the node from the graph and add it to the simplified nodes
//as if nothing happened. The actual spill happens when select&coloring
bool Coloring::spill() {
    #ifdef DEBUG
    cout << "enter spill  ";
    #endif
    if (graph.empty()) return false;
    
    // 选择溢出候选节点（使用简单启发式：最高度数）
    int spill_candidate = -1;
    size_t max_degree = 0;
    
    for (const auto& [node, neighbors] : graph) {
        if (!isMachineReg(node) && !coalescedMoves.count(node)) {
            size_t degree = neighbors.size();
            if (degree > max_degree) {
                max_degree = degree;
                spill_candidate = node;
            }
        }
    }
    
    if (spill_candidate != -1) {
        // 将节点压入栈并从图中移除（实际溢出在select阶段决定）
        simplifiedNodes.push(spill_candidate);
        eraseNode(spill_candidate);
        // 删除所有有关的movePair
        if(isMove(spill_candidate)){
            for (auto it = movePairs.begin(); it != movePairs.end(); ) {
                if (it->first == spill_candidate || it->second == spill_candidate) {
                    it = movePairs.erase(it);
                }else{
                    it++;
                }
            }
        }
        
        #ifdef DEBUG
        cout << "Potential spill: " << spill_candidate << endl;
        // cout << "**********************************" << endl;
        // cout << printColoring();
        #endif
        
        return true;
    }
    
    return false;
}

//now try to select the registers for the nodes
//finally check the validity of the coloring
bool Coloring::select() {
    #ifdef DEBUG
    cout << "enter select" << endl;
    cout << printColoring();
    #endif

    // 给图中剩余的节点着色
    for (const auto& [node, neighbors] : graph) {
        if (isMachineReg(node)){
            colors[node] = node;
            if(coalescedMoves.count(node)){
                for(const auto& coalesced: coalescedMoves[node]){
                    if(!isAnEdge(graph, coalesced, node))
                        colors[coalesced] = node;
                }
            }
            continue;
        }
        set<int> used_colors_1;
        set<int> neighbors_1 = getNeighbors(node); 
        for (int neighbor : neighbors_1) {
            if (colors.count(neighbor)) {
                used_colors_1.insert(colors[neighbor]);
            }
        }
        for (int c = 0; c < k; c++) {
            if (used_colors_1.find(c) == used_colors_1.end()) {
                colors[node] = c;
                break;
            }
        }
    }

    graph = ig->graph;

    // Process nodes in reverse order of simplification
    while (!simplifiedNodes.empty()) {
        int node = simplifiedNodes.top();
        simplifiedNodes.pop();

        // if(colors.count(node))
        //     continue;
        
        // 收集邻居已使用的颜色
        set<int> used_colors;
        set<int> neighbors = getNeighbors(node); 
        for (int neighbor : neighbors) {
            if (colors.count(neighbor)) {
                used_colors.insert(colors[neighbor]);
            }
        }
        
        // 尝试分配可用颜色
        for (int c = 0; c < k; c++) {
            if (used_colors.find(c) == used_colors.end()) {
                colors[node] = c;
                break;
            }
        }
        
        // 分配失败，标记为实际溢出
        if (!colors.count(node)) {
            spilled.insert(node);
            colors[node] = 0;
            eraseNode(node);
            #ifdef DEBUG
            cout << "Actual spill: " << node << endl;
            #endif

            // select();
        } else {
            if(coalescedMoves.count(node)){
                for(const auto& coalesced: coalescedMoves[node]){
                    if(!isAnEdge(graph, coalesced, node))
                        colors[coalesced] = colors[node];
                }
            }
        }
    }

    return checkColoring();
}
