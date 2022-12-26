//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Lukas Pusch, lukas@lpusch.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------
#ifndef __LIST_H__
#define __LIST_H__

template<class T, class... Args>
struct node_s {
    typedef T dT;
    node_s *pre;
    node_s *nxt;
    uint32_t id;
    dT d;
    node_s() : pre(NULL), nxt(NULL), d() {}
    node_s(Args... args) : id(0), pre(NULL), nxt(NULL), d(args...) {}
};

template<int MAX_NUM, class T, class... Args>
class llist {
    typedef node_s<T, Args...> elmType;
    typedef T dataType;
    public:
        llist() : root(mPool) {
            root = NULL;
            elmType *p = mPool;
            for(uint32_t i = 0; i < MAX_NUM; i++) {
                p->id = i;
                p++;
            }
            mFill = mMax = 0;
        }

        elmType *add(Args... args) {
            elmType *p = root, *t;
            if(NULL == (t = getFreeNode()))
                return NULL;
            if(++mFill > mMax)
                mMax = mFill;

            if(NULL == root) {
                p = root = t;
                p->pre = p;
                p->nxt = p;
            }
            else {
                p = root->pre;
                t->pre = p;
                p->nxt->pre = t;
                t->nxt = p->nxt;
                p->nxt = t;
            }
            t->d = dataType(args...);
            return p;
        }

        elmType *getFront() {
            return root;
        }

        elmType *get(elmType *p) {
            p = p->nxt;
            return (p == root) ? NULL : p;
        }

        elmType *rem(elmType *p) {
            if(NULL == p)
                return NULL;
            elmType *t = p->nxt;
            p->nxt->pre = p->pre;
            p->pre->nxt = p->nxt;
            if(root == p)
                root = NULL;
            p->nxt = NULL;
            p->pre = NULL;
            p = NULL;
            mFill--;
            return (NULL == root) ? NULL : ((t == root) ? NULL : t);
        }

        uint16_t getFill(void) {
            return mFill;
        }

        uint16_t getMaxFill(void) {
            return mMax;
        }

    protected:
        elmType *root;

    private:
        elmType *getFreeNode(void) {
            elmType *n = mPool;
            for(uint32_t i = 0; i < MAX_NUM; i++) {
                if(NULL == n->nxt)
                    return n;
                n++;
            }
            return NULL;
        }

        elmType mPool[MAX_NUM];
        uint16_t mFill, mMax;
};

#endif /*__LIST_H__*/
