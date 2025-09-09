#pragma once

#include "ArrowStore.h"
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <limits>

// Use as::DataFormat from ArrowStore.h instead of local enum

namespace gexpr {


struct AttrData {
    void* data;
    size_t length;
    as::DataFormat type;
};

enum AttributeOp {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_SIN,
    OP_COS,
    OP_TAN,
    OP_LOG,
    OP_SQRT,
    OP_ABS,
    OP_POW,
    OP_EXP
};

template<typename T>
struct TypeTraits {
    using type = T;
    static constexpr bool is_integer = std::is_integral_v<T>;
    static constexpr bool is_floating = std::is_floating_point_v<T>;
};

template<as::DataFormat fmt>
struct FormatToType {};

template<> struct FormatToType<as::DataFormat::FORMAT_U8>  { using type = int8_t; };
template<> struct FormatToType<as::DataFormat::FORMAT_U16> { using type = int16_t; };
template<> struct FormatToType<as::DataFormat::FORMAT_U32> { using type = int32_t; };
template<> struct FormatToType<as::DataFormat::FORMAT_U64> { using type = int64_t; };
template<> struct FormatToType<as::DataFormat::FORMAT_R32> { using type = float; };
template<> struct FormatToType<as::DataFormat::FORMAT_R64> { using type = double; };

template<typename T>
constexpr as::DataFormat TypeToFormat() {
    if constexpr (std::is_same_v<T, int8_t>) return as::DataFormat::FORMAT_U8;
    else if constexpr (std::is_same_v<T, int16_t>) return as::DataFormat::FORMAT_U16;
    else if constexpr (std::is_same_v<T, int32_t>) return as::DataFormat::FORMAT_U32;
    else if constexpr (std::is_same_v<T, int64_t>) return as::DataFormat::FORMAT_U64;
    else if constexpr (std::is_same_v<T, float>) return as::DataFormat::FORMAT_R32;
    else if constexpr (std::is_same_v<T, double>) return as::DataFormat::FORMAT_R64;
}

template<typename T>
inline T safe_cast(double value) {
    if constexpr (std::is_integral_v<T>) {
        // Round the value first
        double rounded = std::round(value);
        
        // Clamp to the valid range for this type
        if constexpr (std::is_unsigned_v<T>) {
            if (rounded < 0.0) return T(0);
            if (rounded > static_cast<double>(std::numeric_limits<T>::max())) {
                return std::numeric_limits<T>::max();
            }
        } else {
            if (rounded < static_cast<double>(std::numeric_limits<T>::min())) {
                return std::numeric_limits<T>::min();
            }
            if (rounded > static_cast<double>(std::numeric_limits<T>::max())) {
                return std::numeric_limits<T>::max();
            }
        }
        
        return static_cast<T>(rounded);
    } else {
        return static_cast<T>(value);
    }
}

struct OperationInfo {
    bool is_binary;
    const char* name;
};

const OperationInfo* get_operation_info(AttributeOp op);
bool vector_compute(AttributeOp operation, AttrData* result, const AttrData* first, const AttrData* second);
bool convert_vector(AttrData* dst, const AttrData* src);

    
}