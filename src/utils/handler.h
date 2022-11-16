//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Lukas Pusch, lukas@lpusch.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <memory>
#include <functional>
#include <list>

template<class TYPE>
class Handler {
    public:
        Handler() {}

        void addListener(TYPE f) {
            mList.push_back(f);
        }

        /*virtual void notify(void) {
            for(typename std::list<TYPE>::iterator it = mList.begin(); it != mList.end(); ++it) {
               (*it)();
            }
        }*/

    protected:
        std::list<TYPE> mList;
};

#endif /*__HANDLER_H__*/
