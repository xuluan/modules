#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <exception>
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include "scale.h"
#include "scale_common.h"

void get_agc_data(
    const std::vector<std::vector<double>>& in_data, 
    float dt, 
    float window_size,
    std::vector<std::vector<double>>& out_data)
{

    size_t data_width = in_data.size();

    if (data_width < 1) {
        throw std::runtime_error("get_agc_data() failed: invalid 'in_data' parameter, width is 0.");
    }

    size_t data_height = in_data[0].size();

    if (data_height < 1) {
        throw std::runtime_error("get_agc_data() failed: invalid 'in_data' parameter, height is 0");  
    }
    
    out_data.clear();

    int radius = std::max(1, static_cast<int>((window_size + 0.00001) / dt / 2));

    for (size_t x = 0; x < data_width; ++x) {
        std::vector<double> trace(data_height, 0);
        double sum_ = 0;
        int n = 0;

        for (size_t y = 0; y < std::min(static_cast<size_t>(radius), data_height); ++y) {
            sum_ += std::abs(in_data[x][y]);
            n += 1;
        }

        for (size_t y = 0; y < data_height; ++y) {
            if (y > radius) {
                sum_ -= std::abs(in_data[x][y - radius - 1]);
                n -= 1;
            }
            if (y + radius < data_height) {
                sum_ += std::abs(in_data[x][y + radius]);
                n += 1;
            }

            double value = (sum_ != 0 && n > 0) ? (in_data[x][y] * n) / sum_ : 0;
            trace[y] = value;
        }

        out_data.push_back(trace);
    }

}


bool get_scale_data_agc(Scale *my_data)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = my_data->logger;
    void* trc_data = my_data->trc_data;
    size_t grp_size = my_data->grp_size;
    size_t trc_len = my_data->trc_len;
    as::DataFormat trc_fmt = my_data->trc_fmt;

    // gd_logger.LogInfo(my_logger, "get_scale_data_agc");

    if (trc_data == nullptr) {
        gd_logger.LogError(my_logger, "nullptr error of trc_data");
        return false;
    }

    gd_logger.LogInfo(my_logger, "sinterval={}", my_data->sinterval);
    gd_logger.LogInfo(my_logger, "window_size={}", my_data->window_size);

    std::vector<std::vector<double>> trc_orig, trc_agc;

    
    try {
        // Converting trc_data to two-dimensional double array
        conv_void_ptr_to_2d_double(trc_data, grp_size, trc_len, trc_fmt, trc_orig);

        // call the AGC algorithm 
        get_agc_data(trc_orig, my_data->sinterval, my_data->window_size, trc_agc);

        // Converting two-dimensional double array to trc_data
        conv_2d_double_to_void_ptr(trc_data, grp_size, trc_len, trc_fmt, trc_agc);

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        return false;
    }

    return true;
}
