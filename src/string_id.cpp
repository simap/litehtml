#include "html.h"
#include "string_id.h"
#include <cassert>

#ifndef LITEHTML_NO_THREADS
#include <mutex>
static std::mutex mutex;
#define lock_guard std::lock_guard<std::mutex> lock(mutex)
#else
#define lock_guard
#endif

namespace litehtml
{

    static std::map<std::string, string_id> map;
    static std::vector<std::string>         array;

    static int init()
    {
        string_vector names;
        split_string(initial_string_ids, names, ",");
        for(auto& name : names)
        {
            trim(name);
            assert(name[0] == '_' && name.back() == '_');
            name = name.substr(1, name.size() - 2);           // _border_color_ -> border_color
            std::replace(name.begin(), name.end(), '_', '-'); // border_color   -> border-color
            _id(name); // this will create association _border_color_ <-> "border-color"
        }
        return 0;
    }
    static int dummy = init();

    const string_id empty_id = _id("");
    const string_id star_id  = _id("*");
    // ids that exist after static initialization of this file; defined after empty_id and star_id, so it
    // counts them. No other translation unit creates an id at static initialization.
    static const size_t static_id_count = array.size();

    size_t reset_string_ids()
    {
        lock_guard;
        size_t dropped = 0;
        if(array.size() > static_id_count)
        {
            dropped = array.size() - static_id_count;
            for(size_t i = static_id_count; i < array.size(); i++)
            {
                map.erase(array[i]);
            }
            array.resize(static_id_count);
        }
        // release the capacity too (and the growth slack of static initialization); a page grows the array
        // to the next power of two above its id count, 196,608 B kept on the device after Wikipedia, and
        // std::vector::shrink_to_fit does not release it there (see litehtml::shrink_to_fit)
        litehtml::shrink_to_fit(array);
        return dropped;
    }

    size_t string_id_count()
    {
        lock_guard;
        return array.size();
    }

    string_id _id(const std::string& str)
    {
        lock_guard;
        auto it = map.find(str);
        if(it != map.end())
        {
            return it->second;
        }
        // else: str not found, add it to the array and the map
        array.push_back(str);
        return map[str] = static_cast<string_id>(array.size() - 1);
    }

    const std::string& _s(string_id id)
    {
        lock_guard;
        return array[id];
    }

} // namespace litehtml
