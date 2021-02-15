// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ARRAY2_HPP
#define ARRAY2_HPP

#include <functional>

#include "pos.hpp"
#include "rect.hpp"

// Two dimensional dynamic array class
template <typename T>
class Array2
{
public:
        Array2(const P& dims)
        {
                resize(dims);
        }

        Array2(const int w, const int h)
        {
                resize({w, h});
        }

        Array2(const Array2<T>& other)
        {
                resize_no_init(other.m_dims);

                std::copy(
                        std::begin(other),
                        std::end(other),
                        std::begin(*this));
        }

        Array2(Array2<T>&& other) :
                m_data(other.m_data),
                m_dims(other.m_dims)
        {
                other.m_data = nullptr;
                other.m_dims = {0, 0};
        }

        ~Array2()
        {
                delete[] m_data;
        }

        Array2<T>& operator=(const Array2<T>& other)
        {
                if (&other == this)
                {
                        return *this;
                }

                if (m_dims != other.m_dims)
                {
                        resize_no_init(other.m_dims);
                }

                std::copy(
                        std::begin(other),
                        std::end(other),
                        std::begin(*this));

                return *this;
        }

        Array2<T>& operator=(Array2<T>&& other)
        {
                if (&other == this)
                {
                        return *this;
                }

                delete[] m_data;

                m_data = other.m_data;
                m_dims = other.m_dims;

                other.m_data = nullptr;
                other.m_dims = {0, 0};

                return *this;
        }

        T& at(const P& p)
        {
                return get_element_ref(p);
        }

        const T& at(const P& p) const
        {
                return get_element_ref(p);
        }

        T& at(const int x, const int y)
        {
                return get_element_ref({x, y});
        }

        const T& at(const int x, const int y) const
        {
                return get_element_ref({x, y});
        }

        T& at(const size_t idx)
        {
                return m_data[idx];
        }

        const T& at(const size_t idx) const
        {
                return m_data[idx];
        }

        T* begin() const
        {
                return m_data;
        }

        T* end() const
        {
                return m_data + length();
        }

        void resize(const P& dims)
        {
                m_dims = dims;

                const size_t len = length();

                delete[] m_data;

                m_data = nullptr;

                if (len > 0)
                {
                        m_data = new T[len]();
                }
        }

        void resize(const int w, const int h)
        {
                resize({w, h});
        }

        void resize(const P& dims, const T value)
        {
                resize_no_init(dims);

                std::fill_n(m_data, length(), value);
        }

        void resize(const int w, const int h, const T value)
        {
                resize({w, h}, value);
        }

        void resize_no_init(const P& dims)
        {
                m_dims = dims;

                const size_t len = length();

                delete[] m_data;

                m_data = nullptr;

                if (len > 0)
                {
                        m_data = new T[len];
                }
        }

        void rotate_cw()
        {
                const P my_dims(dims());

                Array2<T> rotated(my_dims.y, my_dims.x);

                for (int x = 0; x < my_dims.x; ++x)
                {
                        for (int y = 0; y < my_dims.y; ++y)
                        {
                                const size_t my_idx = pos_to_idx(x, y);

                                rotated.at(my_dims.y - 1 - y, x) =
                                        m_data[my_idx];
                        }
                }

                *this = rotated;
        }

        void rotate_ccw()
        {
                const P my_dims(dims());

                Array2<T> rotated(my_dims.y, my_dims.x);

                for (int x = 0; x < my_dims.x; ++x)
                {
                        for (int y = 0; y < my_dims.y; ++y)
                        {
                                const size_t my_idx = pos_to_idx(x, y);

                                rotated.at(y, my_dims.x - 1 - x) =
                                        m_data[my_idx];
                        }
                }

                *this = rotated;
        }

        void flip_hor()
        {
                const P d(dims());

                for (int x = 0; x < d.x / 2; ++x)
                {
                        for (int y = 0; y < d.y; ++y)
                        {
                                const size_t idx_1 = pos_to_idx(x, y);
                                const size_t idx_2 = pos_to_idx(d.x - 1 - x, y);

                                std::swap(m_data[idx_1], m_data[idx_2]);
                        }
                }
        }

        void flip_ver()
        {
                const P d(dims());

                for (int x = 0; x < d.x; ++x)
                {
                        for (int y = 0; y < d.y / 2; ++y)
                        {
                                const size_t idx_1 = pos_to_idx(x, y);
                                const size_t idx_2 = pos_to_idx(x, d.y - 1 - y);

                                std::swap(m_data[idx_1], m_data[idx_2]);
                        }
                }
        }

        void clear()
        {
                delete[] m_data;

                m_dims.set(0, 0);
        }

        size_t length() const
        {
                return m_dims.x * m_dims.y;
        }

        const P& dims() const
        {
                return m_dims;
        }

        int w() const
        {
                return m_dims.x;
        }

        int h() const
        {
                return m_dims.y;
        }

        R rect() const
        {
                return R({0, 0}, m_dims - 1);
        }

        T* data()
        {
                return m_data;
        }

        const T* data() const
        {
                return m_data;
        }

private:
        T& get_element_ref(const P& p) const
        {
                const size_t idx = pos_to_idx(p);

                return m_data[idx];
        }

        size_t pos_to_idx(const P& p) const
        {
                return (p.x * m_dims.y) + p.y;
        }

        size_t pos_to_idx(const int x, const int y) const
        {
                return pos_to_idx({x, y});
        }

        T* m_data {nullptr};
        P m_dims {0, 0};
};

#endif  // ARRAY2_HPP
