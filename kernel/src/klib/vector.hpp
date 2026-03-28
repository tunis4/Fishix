#pragma once

#include <klib/cstdlib.hpp>
#include <klib/common.hpp>

namespace klib {

template<typename T>
class Vector {
private:
    T* m_buffer = nullptr;
    usize m_size = 0;
    usize m_capacity = 0;

    void destroy_range(usize from, usize to) {
        for (usize i = from; i < to; ++i)
            m_buffer[i].~T();
    }

    void reserve_internal(usize new_cap) {
        if (new_cap <= m_capacity)
            return;

        usize cap = m_capacity ? m_capacity : 1;
        while (cap < new_cap)
            cap *= 2;

        T* new_buf = (T*)klib::malloc(sizeof(T) * cap);

        // move existing objects
        for (usize i = 0; i < m_size; ++i) {
            new (new_buf + i) T(klib::move(m_buffer[i]));
            m_buffer[i].~T();
        }

        klib::free(m_buffer);

        m_buffer = new_buf;
        m_capacity = cap;
    }

public:

    /* =========================
        Constructors
       ========================= */

    Vector() = default;

    explicit Vector(usize reserve) {
        reserve_internal(reserve);
    }

    Vector(const Vector& other) {
        reserve_internal(other.m_size);

        for (usize i = 0; i < other.m_size; ++i)
            new (m_buffer + i) T(other.m_buffer[i]);

        m_size = other.m_size;
    }

    Vector(Vector&& other) noexcept {
        m_buffer = other.m_buffer;
        m_size = other.m_size;
        m_capacity = other.m_capacity;

        other.m_buffer = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    ~Vector() {
        clear();
        klib::free(m_buffer);
    }

    /* =========================
        Assignment
       ========================= */

    Vector& operator=(const Vector& other) {
        if (this == &other)
            return *this;

        clear();
        reserve_internal(other.m_size);

        for (usize i = 0; i < other.m_size; ++i)
            new (m_buffer + i) T(other.m_buffer[i]);

        m_size = other.m_size;
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        clear();
        klib::free(m_buffer);

        m_buffer = other.m_buffer;
        m_size = other.m_size;
        m_capacity = other.m_capacity;

        other.m_buffer = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;

        return *this;
    }

    /* =========================
        Element Access
       ========================= */

    T& operator[](usize index) {
        return m_buffer[index];
    }

    const T& operator[](usize index) const {
        return m_buffer[index];
    }

    T& back() {
        return m_buffer[m_size - 1];
    }

    /* =========================
        Capacity
       ========================= */

    void reserve(usize cap) {
        reserve_internal(cap);
    }

    usize size() const { return m_size; }
    usize capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

    T* data() { return m_buffer; }
    const T* data() const { return m_buffer; }

    /* =========================
        Modifiers
       ========================= */

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        reserve_internal(m_size + 1);

        new (m_buffer + m_size)
            T(klib::forward<Args>(args)...);

        return m_buffer[m_size++];
    }

    T& push_back(const T& v) {
        return emplace_back(v);
    }

    T& push_back(T&& v) {
        return emplace_back(klib::move(v));
    }

    void pop_back() {
        if (!m_size) return;
        m_buffer[--m_size].~T();
    }

    void resize(usize new_size) {
        if (new_size < m_size) {
            destroy_range(new_size, m_size);
        } else {
            reserve_internal(new_size);
            for (usize i = m_size; i < new_size; ++i)
                new (m_buffer + i) T();
        }

        m_size = new_size;
    }

    void clear() {
        destroy_range(0, m_size);
        m_size = 0;
    }

    void remove(usize index) {
        if (index >= m_size) return;

        m_buffer[index].~T();

        // Shift elements
        for (usize i = index; i < m_size - 1; ++i) {
            new (m_buffer + i) T(klib::move(m_buffer[i + 1]));
            m_buffer[i + 1].~T();
        }

        --m_size;
    }

    /* =========================
        Iterators
       ========================= */

    struct iterator {
        T* ptr;

        iterator(T* p) : ptr(p) {}

        T& operator*() const { return *ptr; }
        T* operator->() const { return ptr; }

        iterator& operator++() { ++ptr; return *this; }
        iterator operator++(int){ iterator t=*this; ++ptr; return t; }

        iterator& operator--() { --ptr; return *this; }

        bool operator==(const iterator& o) const { return ptr == o.ptr; }
        bool operator!=(const iterator& o) const { return ptr != o.ptr; }
    };

    iterator begin() { return iterator(m_buffer); }
    iterator end() { return iterator(m_buffer + m_size); }
};

} // namespace klib