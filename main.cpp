#include <cstring>
#include <iostream>
#include <algorithm>
#include <bitset>
#include <climits>
#include <cstdio>
#include <deque>//双端队列
#include <functional>
#include <iterator>
#include <memory.h>
#include <numeric>
#include <set>
#include <string>
#include <stack>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;


struct TreeNode {
    int val;
    TreeNode *left;
    TreeNode *right;
    TreeNode() : val(0), left(nullptr), right(nullptr) {}
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
    TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
};

struct ListNode {
    int val;
    ListNode *next;
    ListNode() : val(0), next(nullptr) {}
    ListNode(int x) : val(x), next(nullptr) {}
    ListNode(int x, ListNode *next) : val(x), next(next) {}
};

class Solution {
public:

    vector<vector<int>> ans;
    vector<int> path;

    void dfs(){

        
    }
    vector<vector<int>> allPathsSourceTarget(vector<vector<int>>& graph) {

    }
};

int main(){
    


    return 0;
}