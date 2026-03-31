#ifndef __MAP_HPP__
#define __MAP_HPP__

#include <map>
#include <mutex>
#include <stdexcept>

/**
 * 检查锁状态的内联函数
 * @param lock 要检查的unique_lock
 * @param mutex2 期望的互斥锁指针
 * @throws std::logic_error 如果锁未锁定或锁定错误的互斥锁
 */
inline void check_lock(std::unique_lock<std::mutex> &lock, std::mutex *mutex2) {
    // 检查锁是否已锁定
    if (!lock.owns_lock()) {
        throw std::logic_error("Map: 锁必须处于锁定状态");
    }
    // 检查锁是否由指定的互斥锁锁定
    if (lock.mutex() != mutex2) {
        throw std::logic_error("Map: 互斥锁必须相同");
    }
}

/**
 * 线程安全的映射模板类
 * @tparam K 键类型
 * @tparam V 值类型  
 * @tparam Compare 键比较函数类型，默认为std::less<K>
 */
template <typename K, typename V, typename Compare = std::less<K> >
class Map {
private:
    std::map<K, V, Compare> map_;  ///< 底层标准映射容器
    std::mutex mutex_;             ///< 保护映射访问的互斥锁

public:
    Map() {}
    ~Map() {}

    /**
     * 获取底层互斥锁引用
     * @return 互斥锁的引用
     */
    std::mutex &get_mutex() {
        return mutex_;
    }

    /**
     * 检查映射是否为空（只读操作）
     * @return 如果映射为空返回true，否则返回false
     */
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.empty();
    }

    /**
     * 插入或赋值操作
     * @param key 要插入或更新的键
     * @param value 要插入或设置的值
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 插入结果的pair（迭代器和bool指示是否插入新元素）
     */
    auto insert_or_assign(const K &key, V value, std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.insert_or_assign(key, std::move(value));
    }

    /**
     * 根据键删除元素
     * @param key 要删除的元素的键
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 被删除元素的数量（对于映射通常为0或1）
     */
    auto erase(const K &key, std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.erase(key);
    }

    /**
     * 根据迭代器删除元素
     * @param it 指向要删除元素的迭代器
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 被删除元素之后的迭代器
     */
    auto erase(typename std::map<K, V, Compare>::iterator it, std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.erase(it);
    }

    /**
     * 根据键访问元素（带边界检查）
     * @param key 要访问的元素的键
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 对应键的值的引用
     * @throws std::out_of_range 如果键不存在
     */
    V &at(const K &key, std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.at(key);
    }

    /**
     * 清空映射中的所有元素
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 总是返回true
     */
    bool clear(std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        map_.clear();
        return true;
    }

    /**
     * 查找具有指定键的元素
     * @param key 要查找的键
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 指向找到元素的迭代器，如果未找到则返回end()
     */
    auto find(const K &key, std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.find(key);
    }

    /**
     * 获取指向映射第一个元素的迭代器
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 指向第一个元素的迭代器
     */
    auto begin(std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.begin();
    }

    /**
     * 获取指向映射末尾的迭代器
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 指向末尾的迭代器
     */
    auto end(std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.end();
    }

    /**
     * 检查指定键是否存在
     * @param key 要检查的键
     * @param lock 必须已锁定此映射互斥锁的unique_lock
     * @return 如果键存在返回true，否则返回false
     */
    bool check_exist(const K &key, std::unique_lock<std::mutex> &lock) {
        check_lock(lock, &mutex_);
        return map_.find(key) != map_.end();
    }
};

#endif