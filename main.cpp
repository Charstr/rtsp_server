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

class LRUCache {
public:
    list<pair<int, int>> _lru;
    unordered_map<int, list<pair<int, int>>::iterator> _cached;
    // key iter
    LRUCache(int capacity) {
        _capacity = capacity;
    }
    
    int get(int key) {
        auto it = _cached.find(key);
        if(it!=_cached.end()){
            _lru.splice(_lru.begin(), _lru, it->second);
            return it->second->second;
        }
        return -1;
    }
    
    void put(int key, int value) {

    }
    int _capacity;
};

int main(){
    


    return 0;
}