"""Make the pinned intrusive-list API self-contained and instantiate its wrappers.

The original CList.cpp link algorithms are copied byte-for-byte (LF normalized).
"""
INPUTS={
    'CList.cpp':'4e6e9cc167e6f7f996b6d77d273900ca38dea10584f93214a29769f5d5d953dd',
    'CList.hpp':'26c89e0c65a8c5cb493315431cf1360567328f3ed89afd0f9a8c478dfc57e19d',
}

def patch_list(files):
    text=files['CList.hpp'].decode()
    for old,new in (
        ('#define CLIST_HPP','#define CLIST_HPP\n#include <cstddef>'),
        ('this->sub(item)','this->Sub(item)'),
        ('this->Insert(item, to_item)','this->Insert(head, to_item)'),
    ):
        if text.count(old)!=1:raise ValueError('Pinned CList header changed: '+old)
        text=text.replace(old,new,1)
    files['CList.hpp']=text.encode()
