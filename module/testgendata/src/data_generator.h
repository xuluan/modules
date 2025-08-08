#pragma once
#include <string>
#include <fstream>
#include <random>
#include <cstdint>
#include "ArrowStore.h"

struct RandomData {
    as::DataFormat type;
    float min;
    float max;
};

struct SequenceData {
    as::DataFormat type;
    float min;
    float max;
    float step;
};

class DataGenerator {
private:
    std::string error_message_;
    std::string output_filename_;
    std::mt19937 random_engine_;
    
    template<typename T>
    bool generate_random_values(void* data, const RandomData& config, int length);
    
    template<typename T>
    bool generate_sequence_values(void* data, const SequenceData& config, int length);
    
    template<typename T>
    bool save_data_to_file(const T* data, int length);
    
    size_t get_type_size(as::DataFormat format);
    
public:
    DataGenerator(const std::string& output_filename);
    
    bool gen_random_data(void* data, const RandomData& config, int length);
    bool gen_sequence_data(void* data, const SequenceData& config, int length);
    
    const std::string& get_error_message() const { return error_message_; }
    void set_output_filename(const std::string& filename) { output_filename_ = filename; }
    const std::string& get_output_filename() const { return output_filename_; }
};
