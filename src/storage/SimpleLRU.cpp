#include <memory>
#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string& key, const std::string &value)
{
    auto it = _lru_index.find(key);
    return _put(key, value, it);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) 
{
    auto it = _lru_index.find(key);
    if(it != _lru_index.end())
    {
       return false;
    }
    return _put(key, value, it);
}

bool SimpleLRU::_put(const std::string &key, const std::string &value, 
    node_map::iterator it)
{
    std::size_t put_size = key.length() + value.length();
    if(put_size > _max_size)
    {
       return false; //need log?
    }
    std::size_t match_key_size = 0;
    //если мы заменяем ключ значение, то общий размер считаем за вычетом заменяемого
    if(it != _lru_index.end())
    {
        match_key_size = it->second.get().key.length() + it->second.get().value.length();
    }
    while(current_size - match_key_size + put_size > _max_size)
    {
        if(_lru_head->key == key)
        {
            match_key_size = 0;
        }
        current_size -= _lru_head->key.length() + _lru_head->value.length();
        lru_node* new_head = _lru_head->next.get();
        new_head->prev = _lru_head->prev;
        _lru_head->next.release();
        _lru_index.erase(_lru_head->key);
        _lru_head.reset(new_head);
    }
    //Добавляем ключ
    if(_lru_head)
    {
        if(it != _lru_index.end())
        {
            auto& old_value = it->second.get().value;
            current_size -= key.length() + old_value.length();
            old_value = value;
        }
        else
        {
            auto freshest = _lru_head->prev;// regular ptr;
            freshest->next.reset(new lru_node(key,value));
            freshest->next->prev = freshest;
            _lru_head->prev =  freshest->next.get();
            _lru_index.emplace(_lru_head->prev->key, *_lru_head->prev);
        }
    }
    else
    {
        _lru_head.reset(new lru_node(key,value));
        _lru_head->prev = _lru_head.get();
        _lru_index.emplace(_lru_head->prev->key, *_lru_head->prev);
    }
    current_size += key.length() + value.length();
    return true;
}
// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value)
{
    auto it = _lru_index.find(key);
    if(it == _lru_index.end())
    {
       return false;
    }
    return _put(key, value, it);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key)
{
    auto it = _lru_index.find(key);
    if(it == _lru_index.end())
    {
        return false;
    }
    auto& lru_node = it->second.get();
    _lru_index.erase(key);
    current_size -= key.length() + lru_node.value.length();
    auto prev = lru_node.prev;
    auto next = lru_node.next.get();
    if(next)
    {
        next->prev = prev;
    }
    else
    {
        _lru_head->prev = prev;
    }
    if(_lru_head->key == key)
    {
        _lru_head->next.release();
        _lru_head.reset(next);
    }
    else
    {
        prev->next.reset(next);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value)
{
    auto it = _lru_index.find(key);
    if(it == _lru_index.end())
    {
       return false;
    }
    value = it->second.get().value;
    return true;
}

} // namespace Backend
} // namespace Afina
