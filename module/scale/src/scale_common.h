#ifndef SCALE_COMMON_H
#define SCALE_COMMON_H

#include <string>

void conv_void_ptr_to_2d_double(void* trc_data, size_t grp_size, size_t trc_len, as::DataFormat trc_fmt,
    std::vector<std::vector<double>>& out_data);

void conv_2d_double_to_void_ptr(void* trc_data, size_t grp_size, size_t trc_len, as::DataFormat trc_fmt,
    const std::vector<std::vector<double>>& in_data);



#endif /* ifndef SCALE_H */
