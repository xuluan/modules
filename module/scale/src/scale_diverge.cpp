#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <algorithm>
#include <cmath>
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include "scale.h"
#include "scale_common.h"

bool get_diverge_data(
    const std::vector<std::vector<double>>& in_data, 
    float o1, float dt, float a, float V,
    std::vector<std::vector<double>>& out_data)
{

    size_t data_width = in_data.size();

    if (data_width < 1) {
        throw std::runtime_error("invalid 'in_data' parameter, width is 0.");
        return false;   
    }

    size_t data_height = in_data[0].size();

    if (data_height < 1) {
        throw std::runtime_error("invalid 'in_data' parameter, height is 0");
        return false;   
    }

    out_data.clear();

    std::vector<double> times(data_height);

    for (size_t y = 0; y < data_height; ++y) {
        times[y] = o1 + dt * y;
    }

    for (size_t x = 0; x < data_width; ++x) {
        std::vector<double> trace(data_height, 0);
        for (size_t y = 0; y < data_height; ++y) {
            double value = in_data[x][y] * std::pow(times[y], a) * V;
            trace[y] = value;
        }
        out_data.push_back(trace);
    }

    return true;
}


bool get_scale_data_diverge(Scale *my_data)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = my_data->logger;
    void* trc_data = my_data->trc_data;
    size_t grp_size = my_data->grp_size;
    size_t trc_len = my_data->trc_len;
    as::DataFormat trc_fmt = my_data->trc_fmt;

    std::vector<std::vector<double>> trc_orig, trc_dvg;

    gd_logger.LogInfo(my_logger, "diverge para: {}, {}, {}, {}", 
        my_data->trc_min, my_data->sinterval, my_data->dvg_a, my_data->dvg_v);

    // Converting trc_data to two-dimensional double array
    try {
        conv_void_ptr_to_2d_double(trc_data, grp_size, trc_len, trc_fmt, trc_orig);
    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        return false;
    }

    // call the diverge algorithm 
    get_diverge_data(trc_orig, my_data->trc_min, my_data->sinterval,
        my_data->dvg_a, my_data->dvg_v, trc_dvg);

    // Converting two-dimensional double array to trc_data
    trc_data = my_data->trc_data;
    try {
        conv_2d_double_to_void_ptr(trc_data, grp_size, trc_len, trc_fmt, trc_dvg);
    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        return false;
    }

    return true;
}
