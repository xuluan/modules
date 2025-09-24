#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include "scale.h"

bool get_scale_data_factor(Scale *my_data)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = my_data->logger;
    void* trc_data = my_data->trc_data;
    size_t grp_size = my_data->grp_size;
    size_t trc_len = my_data->trc_len;
    as::DataFormat trc_fmt = my_data->trc_fmt;

    if (trc_data == nullptr) {
        gd_logger.LogError(my_logger, "nullptr error of trc_data");
        return false;
    }

    // update trace data based on data type
    if (trc_fmt == as::DataFormat::FORMAT_U8) {
        int8_t *trc = static_cast<int8_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, trc,
                        [my_data](auto val) { return val * my_data->factor; }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U16) {
        int16_t *trc = static_cast<int16_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, trc,
                        [my_data](auto val) { return val * my_data->factor; }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U32) {
        int32_t *trc = static_cast<int32_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, trc,
                        [my_data](auto val) { return val * my_data->factor; }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_U64) {
        int64_t *trc = static_cast<int64_t *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, trc,
                        [my_data](auto val) { return val * my_data->factor; }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_R32) {
        float *trc = static_cast<float *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, trc,
                        [my_data](auto val) { return val * my_data->factor; }); 
    } else if (trc_fmt == as::DataFormat::FORMAT_R64) {
        double *trc = static_cast<double *>(trc_data);
        std::transform(trc, trc + grp_size * trc_len, trc,
                        [my_data](auto val) { return val * my_data->factor; }); 
    } else {
        gd_logger.LogWarning(my_logger, "Unknown data format");
        return false;
    }

    return true;
}
