#include <string>
#include <algorithm>
#include <cmath>
#include <exception>
#include "ArrowStore.h"


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


void conv_void_ptr_to_2d_double(void* trc_data, size_t grp_size, size_t trc_len, as::DataFormat trc_fmt,
    std::vector<std::vector<double>>& out_data) 
{

    std::vector<double> double_trc(grp_size*trc_len);
    double *double_trc_data;

    if (trc_data == nullptr) {
        throw std::runtime_error("conv_void_ptr_to_2d_double() failed: nullptr error of trc_data");
    }

    double_trc_data = static_cast<double *>(double_trc.data());
    if (trc_fmt == as::DataFormat::FORMAT_U8) {
        int8_t *trc = static_cast<int8_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, double_trc_data, [](auto val) { return double(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U16) {
        int16_t *trc = static_cast<int16_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, double_trc_data, [](auto val) { return double(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U32) {
        int32_t *trc = static_cast<int32_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, double_trc_data, [](auto val) { return double(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U64) {
        int64_t *trc = static_cast<int64_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, double_trc_data, [](auto val) { return double(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_R32) {
        float *trc = static_cast<float *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, double_trc_data, [](auto val) { return double(val); });
    } else if (trc_fmt == as::DataFormat::FORMAT_R64) {
        double *trc = static_cast<double *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, double_trc_data, [](auto val) { return val; }); 
    } else {
        throw std::runtime_error("conv_void_ptr_to_2d_double() failed: data format is not supported");
    }

    out_data.clear();

    std::vector<double> trc_orig_item;

    // Converting the one-dimensional array to the two-dimensional array
    for (int skey_idx = 0; skey_idx < grp_size; skey_idx++ ) {
        trc_orig_item.clear();
        for ( int trc_idx = 0; trc_idx < trc_len; trc_idx++ ) {
            trc_orig_item.push_back(double_trc_data[skey_idx*trc_len + trc_idx]);
        }
        out_data.push_back(trc_orig_item);
    }

}

void conv_2d_double_to_void_ptr(void* trc_data, size_t grp_size, size_t trc_len, as::DataFormat trc_fmt,
    const std::vector<std::vector<double>>& in_data)
{

    std::vector<double> double_trc(grp_size*trc_len);
    double *double_trc_data;
    double_trc_data = static_cast<double *>(double_trc.data());

    if (trc_data == nullptr) {
        throw std::runtime_error("conv_2d_double_to_void_ptr() failed: nullptr error of trc_data");
    }

    if (in_data.size() != grp_size || in_data.size()<1) {
        throw std::runtime_error("conv_2d_double_to_void_ptr() failed: invalid 'in_data' parameter");
    }
    if (in_data[0].size() != trc_len) {
        throw std::runtime_error("conv_2d_double_to_void_ptr() failed: invalid 'in_data' parameter");
    }

    // Converting two-dimensional array to one-dimensional array
    for (int skey_idx = 0; skey_idx < grp_size; skey_idx++ ) {
        for ( int trc_idx = 0; trc_idx < trc_len; trc_idx++ ) {
            double_trc_data[skey_idx*trc_len + trc_idx] = in_data[skey_idx][trc_idx];
        }
    }

    if (trc_fmt == as::DataFormat::FORMAT_U8) {
        int8_t *trc = static_cast<int8_t *>(trc_data);
        std::transform(double_trc_data, double_trc_data + grp_size * trc_len,
            trc, [](auto val) { return safe_cast<int8_t>(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U16) {
        int16_t *trc = static_cast<int16_t *>(trc_data);
        std::transform(double_trc_data, double_trc_data + grp_size * trc_len,
            trc, [](auto val) { return safe_cast<int16_t>(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U32) {
        int32_t *trc = static_cast<int32_t *>(trc_data);
        std::transform(double_trc_data, double_trc_data + grp_size * trc_len,
            trc, [](auto val) { return safe_cast<int32_t>(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U64) {
        int64_t *trc = static_cast<int64_t *>(trc_data);
        std::transform(double_trc_data, double_trc_data + grp_size * trc_len,
            trc, [](auto val) { return safe_cast<int64_t>(val); }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_R32) {
        float *trc = static_cast<float *>(trc_data);
        std::transform(double_trc_data, double_trc_data + grp_size * trc_len,
            trc, [](auto val) { return safe_cast<float>(val); });
    } else if (trc_fmt == as::DataFormat::FORMAT_R64) {
        double *trc = static_cast<double *>(trc_data);
        std::transform(double_trc_data, double_trc_data + grp_size * trc_len,
            trc, [](auto val) { return val; });
    } else {
        throw std::runtime_error("conv_2d_double_to_void_ptr() failed: data format is not supported");
    }

}
