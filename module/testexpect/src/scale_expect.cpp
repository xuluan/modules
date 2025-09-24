
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "testexpect.h"
#include "attrcalc_expect.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <GdLogger.h>


bool exec_script(std::string const& script_file, int &err_code, std::string &err_msg) {
    // const char* command = "./test/a.py";

    std::string command = script_file + " 2>&1";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        err_msg = "popen() failed.";
        return false;
    }
    char buffer[1024];
    std::string result;
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
        // err_msg = buffer;
    }
    err_msg = result;

    err_code = pclose(pipe);

    // std::cout << "Output:\n" << result << std::endl;
    // std::cout << "Success: " << exit_status << std::endl;

    return err_code == 0;
}

bool check_data_scale_factor(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    void* dst = static_cast<void *> (attr_data.data);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
    
    const char* env_script_path = std::getenv("GEODELITY_TEST_SCRIPT_PATH");

    if (!env_script_path) {
        throw std::runtime_error("check_data fail, 'GEODELITY_TEST_SCRIPT_PATH' is null");   
    }

    std::string script_file, script_cmd;
    std::string err_msg = "";
    bool ret;
    int err_code;

    // get attribute info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int trc_len;
    float trc_min; 
    float trc_max;
    
    job_df.GetAttributeInfo(attr_name.c_str(), trc_fmt, trc_len, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, trc_len);

    script_file = std::string(env_script_path) + "/testexpect_scale.py";

    if(!std::filesystem::exists(script_file)) {
        throw std::runtime_error("check_data fail, " + script_file +  " does not exist");
        return false;
    }

    // example: testexpect_scale.py -m factor --attrname SAMPLES --group_size 10 --trace_length 11 --data_type 4

    script_cmd = script_file;
    script_cmd += " -m factor";
    script_cmd += " --attrname " + attr_name;
    script_cmd += " --group_size " + std::to_string(grp_size);
    script_cmd += " --trace_length " + std::to_string(trc_length);
    script_cmd += " --data_type " + std::to_string(static_cast<int>(trc_fmt)); 
    
    gd_logger.LogInfo(my_data->logger, "script cmd: {}", script_cmd);

    ret = exec_script(script_cmd, err_code, err_msg);

    if( ret ){
        gd_logger.LogInfo(my_data->logger, "exec_script success: {}", err_msg);
    } else {
        gd_logger.LogInfo(my_data->logger, "exec_script fail{}: {}", err_code, err_msg);
    }

    //read '.FCT' file and cmp data

    std::vector<char> dst_data;

    ret = load_data(dst_data, attr_name + ".FCT", attr_data.length * as::get_data_format_size(attr_data.type));

    if(!ret) {
        gd_logger.LogError(my_data->logger, "Load data faile, {}", attr_name + ".FCT");
        return false;
    }

    if(std::memcmp(dst_data.data(), attr_data.data, attr_data.length * as::get_data_format_size(attr_data.type)) != 0) {
        return false;
    } else {
        return true;
    }
}

bool check_data_scale_agc(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    void* dst = static_cast<void *> (attr_data.data);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
    
    const char* env_script_path = std::getenv("GEODELITY_TEST_SCRIPT_PATH");

    if (!env_script_path) {
        throw std::runtime_error("check_data fail, 'GEODELITY_TEST_SCRIPT_PATH' is null");   
    }

    std::string script_file, script_cmd;
    std::string err_msg = "";
    bool ret;
    int err_code;

    // get attribute info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int trc_len;
    float trc_min; 
    float trc_max;
    float trc_interval;
    
    job_df.GetAttributeInfo(attr_name.c_str(), trc_fmt, trc_len, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, trc_len);
    trc_interval = (trc_max - trc_min)/(trc_length -1);

    script_file = std::string(env_script_path) + "/testexpect_scale.py";

    if(!std::filesystem::exists(script_file)) {
        throw std::runtime_error("check_data fail, " + script_file +  " does not exist");
        return false;
    }

    // testexpect_scale.py -m agc --attrname SAMPLES --group_size 10 --trace_length 1001 --data_type 4 --sinterval 5
    script_cmd = script_file;
    script_cmd += " -m agc";
    script_cmd += " --attrname " + attr_name;
    script_cmd += " --group_size " + std::to_string(grp_size);
    script_cmd += " --trace_length " + std::to_string(trc_length);
    script_cmd += " --data_type " + std::to_string(static_cast<int>(trc_fmt)); 
    script_cmd += " --sinterval " + std::to_string(trc_interval);
    
    gd_logger.LogInfo(my_data->logger, "script cmd: {}", script_cmd);

    ret = exec_script(script_cmd, err_code, err_msg);

    if( ret ){
        gd_logger.LogInfo(my_data->logger, "exec_script success: {}", err_msg);
    } else {
        gd_logger.LogInfo(my_data->logger, "exec_script fail{}: {}", err_code, err_msg);
    }

    //read '.AGC' file and compare data

    std::vector<char> dst_data;

    ret = load_data(dst_data, attr_name + ".AGC", attr_data.length * as::get_data_format_size(attr_data.type));

    if(!ret) {
        gd_logger.LogError(my_data->logger, "Load data faile, {}", attr_name + ".AGC");
        return false;
    }

    if(std::memcmp(dst_data.data(), attr_data.data, attr_data.length * as::get_data_format_size(attr_data.type)) != 0) {
        return false;
    } else {
        return true;
    }
}

bool check_data_scale_diverge(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    void* dst = static_cast<void *> (attr_data.data);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
    
    const char* env_script_path = std::getenv("GEODELITY_TEST_SCRIPT_PATH");

    if (!env_script_path) {
        throw std::runtime_error("check_data fail, 'GEODELITY_TEST_SCRIPT_PATH' is null");   
    }

    std::string script_file, script_cmd;
    std::string err_msg = "";
    bool ret;
    int err_code;
    
    // get attribute info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int trc_len;
    float trc_min; 
    float trc_max;
    float trc_interval;
    
    job_df.GetAttributeInfo(attr_name.c_str(), trc_fmt, trc_len, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, trc_len);
    trc_interval = (trc_max - trc_min)/(trc_length -1);

    script_file = std::string(env_script_path) + "/testexpect_scale.py";

    if(!std::filesystem::exists(script_file)) {
        throw std::runtime_error("check_data fail, " + script_file +  " does not exist");
        return false;
    }

    // testexpect_scale.py -m diverge --attrname SAMPLES --group_size 10 --trace_length 1001 --data_type 4 --sinterval 5 --tmin 0

    script_cmd = script_file;
    script_cmd += " -m diverge";
    script_cmd += " --attrname " + attr_name;
    script_cmd += " --group_size " + std::to_string(grp_size);
    script_cmd += " --trace_length " + std::to_string(trc_length);
    script_cmd += " --data_type " + std::to_string(static_cast<int>(trc_fmt)); 
    script_cmd += " --sinterval " + std::to_string(trc_interval);
    script_cmd += " --tmin " + std::to_string(trc_min);
    
    gd_logger.LogInfo(my_data->logger, "script cmd: {}", script_cmd);

    ret = exec_script(script_cmd, err_code, err_msg);

    if( ret ){
        gd_logger.LogInfo(my_data->logger, "exec_script success: {}", err_msg);
    } else {
        gd_logger.LogInfo(my_data->logger, "exec_script fail{}: {}", err_code, err_msg);
    }

    //read '.DVG' file and compare data

    std::vector<char> dst_data;

    ret = load_data(dst_data, attr_name + ".DVG", attr_data.length * as::get_data_format_size(attr_data.type));

    if(!ret) {
        gd_logger.LogError(my_data->logger, "Load data faile, {}", attr_name + ".DVG");
        return false;
    }

    if(std::memcmp(dst_data.data(), attr_data.data, attr_data.length * as::get_data_format_size(attr_data.type)) != 0) {
        return false;
    } else {
        return true;
    }
}
