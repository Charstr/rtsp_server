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

    vector<int> postorderTraversal(TreeNode* root) {
        vector<int> ans;
        if(!root) return ans;
        stack<TreeNode*> stk;
        stk.push(root);
        while(!stk.empty()){
            TreeNode *fr = stk.top();
            stk.pop();
            if(fr){
                stk.push(fr);
                stk.push(nullptr);
                if(fr->right) stk.push(fr->right);
                if(fr->left) stk.push(fr->left);
            }else{
                ans.push_back(stk.top()->val);
                stk.pop();
            }
        }
        return ans;
    }
};

int main(){
    


    return 0;
}