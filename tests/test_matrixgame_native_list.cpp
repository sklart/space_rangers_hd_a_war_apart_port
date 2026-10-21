// Actual pinned CList algorithms. No replacement list/backend implementation.
#include "CList.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

struct Node final:CListInterface<Node> {
    static inline int live{};
    int id;
    explicit Node(int value):id(value){++live;}
    ~Node(){--live;}
};

void Check(Node* head,const std::vector<Node*>& expected) {
    if(expected.empty()){assert(!head);return;}
    assert(head==expected.front());
    assert(head->GetPrev()==expected.back());
    auto* node=head;
    for(std::size_t i=0;i<expected.size();++i) {
        assert(node==expected[i]);
        assert(node->GetPrev()==(i?expected[i-1]:expected.back()));
        node=node->GetNext();
    }
    assert(!node);
}

int main() {
    unsigned operations{};std::uint32_t seed=0x5161;
    {
        std::vector<std::unique_ptr<Node>> pool;
        std::vector<Node*> detached,order;
        for(int i=0;i<24;++i){pool.push_back(std::make_unique<Node>(i));detached.push_back(pool.back().get());}
        Node* head{};
        auto random=[&](){seed=seed*1664525u+1013904223u;return seed;};
        for(unsigned step=0;step<4000;++step) {
            unsigned op=random()%5;
            if(order.empty())op=0;
            if((op==0 || op==3) && detached.empty())op=1;
            if(op==0 || op==3) {
                auto* node=detached.back();detached.pop_back();
                assert(!node->GetNext() && node->GetPrev()==node);
                if(!head){head=node;order.push_back(node);}
                else if(op==0){head->ListAdd(node);order.push_back(node);}
                else {head=head->ListInsert(node,head);order.insert(order.begin(),node);}
            } else if(op==1) {
                const auto i=random()%order.size();auto* node=order[i];
                head=head->ListSub(node);order.erase(order.begin()+i);detached.push_back(node);
                assert(!node->GetNext() && node->GetPrev()==node);
            } else if(op==2) {
                const auto i=random()%order.size();auto* node=order[i];
                head=head->ListMove(node,nullptr);order.erase(order.begin()+i);order.push_back(node);
            } else if(order.size()>1) {
                const auto i=1+random()%(order.size()-1);auto* split=order[i];
                head->ListSplit(split);
                Check(head,{order.begin(),order.begin()+i});
                Check(split,{order.begin()+i,order.end()});
                head->ListAdd(split);
            }
            Check(head,order);++operations;
        }
        while(head)head=head->ListSub(head);
    }
    assert(Node::live==0);
    auto* head=new Node(100);
    for(int i=101;i<111;++i)head->ListAdd(new Node(i));
    assert(Node::live==11);head->ListFree();assert(Node::live==0);
    // Preserve and expose a pinned algorithm limitation rather than silently
    // changing engine semantics while removing a compile blocker. Insert before
    // an interior node does not repair the predecessor's forward link upstream.
    {
        Node a(1),b(2),c(3);a.ListAdd(&b);
        auto* result=a.ListInsert(&c,&b);
        assert(result==&a && a.GetNext()==&b && b.GetPrev()==&c);
        assert(c.GetPrev()==&a && c.GetNext()==&b);
    }
    assert(Node::live==0);
    std::printf("PASS %u original-list operations, wrapper instantiation and complete ListFree; middle-insert upstream limitation reproduced unchanged\n",operations);
}
