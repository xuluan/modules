#include "data_generator.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>

DataGenerator::DataGenerator(const std::string& output_filename) 
    : output_filename_(output_filename), random_engine_(
        std::random_device{}() ^
        std::chrono::steady_clock::now().time_since_epoch().count()) {
}

size_t DataGenerator::get_type_size(as::DataFormat format) {
    switch (format) {
        case as::DataFormat::FORMAT_U8:  return sizeof(int8_t);
        case as::DataFormat::FORMAT_U16: return sizeof(int16_t);
        case as::DataFormat::FORMAT_R32: return sizeof(float);
        case as::DataFormat::FORMAT_U32: return sizeof(int32_t);
        case as::DataFormat::FORMAT_R64: return sizeof(double);
        case as::DataFormat::FORMAT_U64: return sizeof(int64_t);
        default: return 0;
    }
}

template<typename T>
bool DataGenerator::generate_random_values(void* data, const RandomData& config, int length) {
    T* typed_data = static_cast<T*>(data);
    
    if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> dist(static_cast<T>(config.min), static_cast<T>(config.max));
        for (int i = 0; i < length; ++i) {
            typed_data[i] = dist(random_engine_);
        }
    } else {
        std::uniform_int_distribution<T> dist(static_cast<T>(config.min), static_cast<T>(config.max));
        for (int i = 0; i < length; ++i) {
            typed_data[i] = dist(random_engine_);
        }
    }
    
    return save_data_to_file(typed_data, length);
}

template<typename T>
bool DataGenerator::generate_sequence_values(void* data, const SequenceData& config, int length) {
    T* typed_data = static_cast<T*>(data);
    
    T current = static_cast<T>(config.min);
    T step = static_cast<T>(config.step);
    T min_val = static_cast<T>(config.min);
    T max_val = static_cast<T>(config.max);
    
    for (int i = 0; i < length; ++i) {
        typed_data[i] = current;
        
        current += step;
        
        if (step >= 0 && current > max_val) {
            current = min_val;
        } else if (step < 0 && current < min_val) {
            current = max_val;
        }
    }
    
    return save_data_to_file(typed_data, length);
}

template<typename T>
bool DataGenerator::save_data_to_file(const T* data, int length) {
    std::ofstream file(output_filename_, std::ios::binary);
    if (!file.is_open()) {
        error_message_ = "Failed to open output file: " + output_filename_;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data), length * sizeof(T));
    if (!file.good()) {
        error_message_ = "Failed to write data to file: " + output_filename_;
        return false;
    }
    
    file.close();
    return true;
}

bool DataGenerator::gen_random_data(void* data, const RandomData& config, int length) {
    error_message_.clear();
    
    if (!data) {
        error_message_ = "Data pointer is null";
        return false;
    }
    
    if (length <= 0) {
        error_message_ = "Length must be positive";
        return false;
    }
    
    if (config.min > config.max) {
        error_message_ = "Min value cannot be greater than max value";
        return false;
    }
    
    switch (config.type) {
        case as::DataFormat::FORMAT_U8:
            return generate_random_values<int8_t>(data, config, length);
        case as::DataFormat::FORMAT_U16:
            return generate_random_values<int16_t>(data, config, length);
        case as::DataFormat::FORMAT_R32:
            return generate_random_values<float>(data, config, length);
        case as::DataFormat::FORMAT_U32:
            return generate_random_values<int32_t>(data, config, length);
        case as::DataFormat::FORMAT_R64:
            return generate_random_values<double>(data, config, length);
        case as::DataFormat::FORMAT_U64:
            return generate_random_values<int64_t>(data, config, length);
        default:
            error_message_ = "Unknown data format";
            return false;
    }
}

bool DataGenerator::gen_sequence_data(void* data, const SequenceData& config, int length) {
    error_message_.clear();
    
    if (!data) {
        error_message_ = "Data pointer is null";
        return false;
    }
    
    if (length <= 0) {
        error_message_ = "Length must be positive";
        return false;
    }
    
    if (config.min > config.max) {
        error_message_ = "Min value cannot be greater than max value";
        return false;
    }
    
    switch (config.type) {
        case as::DataFormat::FORMAT_U8:
            return generate_sequence_values<int8_t>(data, config, length);
        case as::DataFormat::FORMAT_U16:
            return generate_sequence_values<int16_t>(data, config, length);
        case as::DataFormat::FORMAT_R32:
            return generate_sequence_values<float>(data, config, length);
        case as::DataFormat::FORMAT_U32:
            return generate_sequence_values<int32_t>(data, config, length);
        case as::DataFormat::FORMAT_R64:
            return generate_sequence_values<double>(data, config, length);
        case as::DataFormat::FORMAT_U64:
            return generate_sequence_values<int64_t>(data, config, length);
        default:
            error_message_ = "Unknown data format";
            return false;
    }
}
