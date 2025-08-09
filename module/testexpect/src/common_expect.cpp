

#include "testexpect.h"
#include "common_expect.h"
#include <GdLogger.h>

bool check_data_skip(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    gd_logger.LogInfo(my_data->logger, "SKIP Attr {} check", attr_name);
    return true;
}

bool check_data_same(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    int group_size = my_data->group_size;

    void* dst = attr_data.data;

    void* src = get_and_check_data_valid(my_data, attr_name, attr_data.length, attr_data.type, variables);      
#if DEBUG_DUMP
    printf("check_data_same, dump attr %s \n src:\n", attr_name.c_str());

    pdump(static_cast<char*>(src), attr_data.length * as::get_data_format_size(attr_data.type));
    printf("dst:\n");
    pdump(static_cast<char*>(dst), attr_data.length * as::get_data_format_size(attr_data.type));

#endif
    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }

    gd_logger.LogDebug(my_data->logger, "Dst Attr: {}, Length: {}, Type: {}", attr_name, attr_data.length, as::data_format_to_string(attr_data.type));

    if(std::memcmp(src, dst, attr_data.length * as::get_data_format_size(attr_data.type)) != 0) {

        return false;
    } else {
        return true;
    }
}
