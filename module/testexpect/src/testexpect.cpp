#include "testexpect.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <vector>
#include <cmath>
#include "fort.hpp"
#include <iostream>
#include <utl_yaml_parser.h>
#include <utl_string.h>
#include <fstream>
#include <stdexcept>

#define DEBUG_DUMP 0

#if DEBUG_DUMP
void pdump(char* p, size_t len) {
    if(len > 64) len = 64;
    for (int i = 0; i < len; i += 16) {
        printf("%08x: ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < len) {
                printf("%02x ", (unsigned char)p[i + j]);
            } else {
                printf(" ");
            }
        }
        printf(" ");
        for (int j = 0; j < 16 && i + j < len; j++) {
            char c = p[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }
}
#endif

std::string to_string(CheckPattern c) {
    switch(c) {
        case CheckPattern::SAME:    return "SAME";
        case CheckPattern::ATTR_PLUS_MUL:  return "INLINE+CROSSLINE*2.7";
        default:  throw std::runtime_error("Error CheckPattern");
    }
}

CheckPattern to_checkpattern(std::string s) {
    if(s == "SAME") {
        return CheckPattern::SAME;
    } else if(s == "INLINE+CROSSLINE*2.7") {
        return CheckPattern::ATTR_PLUS_MUL;
    } else {
        throw std::runtime_error("Unknown CheckPattern  : " + s);
    }
}

void load_data (std::vector<char>& data, const std::string& file_name, size_t length) {
    data.resize (length);
    std::ifstream file (file_name, std::ios::binary);
    if (!file) {
        throw std::runtime_error ("load_data: cannot open file:" + file_name);
    }
    file.read (data.data (), length);

    if (file.gcount () != length) {
        throw std::runtime_error ("load_data: read file fail:" + file_name +
        "read:" + std::to_string (file.gcount ()) +
        "expect:" + std::to_string (length));
    }
    file.close();
}

CheckPattern get_pattern(std::string  attr_name, std::vector<AttrConfig>& attrs)
{
    for(int i = 0; i < attrs.size(); ++i) {
        if(attrs[i].name == attr_name) {
            return attrs[i].check_pattern;
        }
    }

    throw std::runtime_error ("get_pattern: cannot find attr:" + attr_name);
}

void* get_and_check_data_valid(Testexpect* my_data, std::string  attr_name, int length, as::DataFormat format, std::map<std::string, AttrData>& variables)
{
    auto it = variables.find(attr_name);
    if (it == variables.end()) {
        throw std::runtime_error("check_data fail: cannot find attribute: " + attr_name);
    } else {
        const AttrData* src_attr_data = const_cast<AttrData*>(&it->second);
        if(src_attr_data->length != length) {
            throw std::runtime_error("check_data fail, attr " + attr_name + "length is not match, expect "  + std::to_string(length)
                + ", but got " + std::to_string(src_attr_data->length)) ;
        }
        if(src_attr_data->type != format) {
            throw std::runtime_error("check_data fail, attr " + attr_name + "datatype is not match, expect " + as::data_format_to_string(format) 
                 + ", but got " + as::data_format_to_string(src_attr_data->type));
        }

        char* src = static_cast<char *> (src_attr_data->data);

        if(!src) {
            throw std::runtime_error("check_data fail, attr " + attr_name + "expect data is null ");
        } 
        
        return src;
    }
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


bool is_equal_float_double(float a, double b) {
    const float epsilon = std::numeric_limits<float>::epsilon() * 100;
    
    double diff = std::fabs(static_cast<double>(a) - b);
    
    return diff < epsilon;
}


bool check_data_plus_mul(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    //check "INLINE+CROSSLINE*2.7"
    int length =  attr_data.length;

    int * pinline = static_cast<int *>(get_and_check_data_valid(my_data, "INLINE", attr_data.length, as::DataFormat::FORMAT_U32, variables));
    int * pcrossline = static_cast<int *>(get_and_check_data_valid(my_data, "CROSSLINE", attr_data.length, as::DataFormat::FORMAT_U32, variables));

    float* dst = static_cast<float *> (attr_data.data);     

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
#if DEBUG_DUMP        
    printf("check_data_plus_mul, dump attr %s \n", attr_name.c_str());
#endif    
    for(int i = 0; i < length; ++i) {
        double c = 2.7;
        double d = c * pcrossline[i] + pinline[i];
#if DEBUG_DUMP        
        printf(" %d, %d, %d, %f == %f \n", i, pinline[i], pcrossline[i], d, dst[i]);
#endif        
        if(!is_equal_float_double(dst[i], d)) {
            throw std::runtime_error("check_data fail, at index " + std::to_string(i) + "expect " + std::to_string(dst[i]) + " but got " + std::to_string(d));
        } 
    }

    return true;
}

bool check_data(Testexpect* my_data, std::string  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables, CheckPattern c)
{
    switch(c) {
        case CheckPattern::SAME:    
            return check_data_same(my_data, attr_name, attr_data, variables);
        case CheckPattern::ATTR_PLUS_MUL:  
            return check_data_plus_mul(my_data, attr_name, attr_data, variables);
        default:  
            throw std::runtime_error("check_data fail: Error CheckPattern");
    }
}

void testexpect_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"testexpect_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "testexpect_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Testexpect* my_data = new Testexpect {};

    my_data->logger = my_logger;

    auto& job_df = df::GeoDataFlow::GetInstance();

    // A handy function to clean up resources if errors happen
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    try {
        // parse job parameters
        gutl::DynamicValue config = gutl::parse(buf);

        //parse primarykey
        auto& primarykey = config["testexpect"]["primarykey"];
        my_data->pkey_name = primarykey.at("name", "primarykey").as_string();
        gutl::UTL_StringToUpperCase(my_data->pkey_name);

        my_data->fpkey = primarykey.at("first", "primarykey").as_int();
        my_data->lpkey = primarykey.at("last", "primarykey").as_int();
        my_data->pkinc = primarykey.at("step", "primarykey").as_int();
        
        gd_logger.LogInfo(my_logger, "Primary Axis: {}, Type: {}, Length: {}, [{} -- {}] "
            , my_data->pkey_name, "int", 1, my_data->fpkey, my_data->lpkey);

        //parse secondarykey
        auto& secondarykey = config["testexpect"]["secondarykey"];

        my_data->skey_name = secondarykey.at("name", "secondarykey").as_string();
        gutl::UTL_StringToUpperCase(my_data->skey_name);

        my_data->fskey = secondarykey.at("first", "secondarykey").as_int();
        my_data->lskey = secondarykey.at("last", "secondarykey").as_int();
        my_data->skinc = secondarykey.at("step", "secondarykey").as_int();
        my_data->num_skey = (my_data->lskey - my_data->fskey) / my_data->skinc + 1;
        
        gd_logger.LogInfo(my_logger, "Secondary Axis: {}, Type: {}, Length: {}, [{} -- {}] "
            , my_data->skey_name, "int", 1, my_data->fskey, my_data->lskey);

        //parse tracekey
        auto& tracekey = config["testexpect"]["tracekey"];

        my_data->trace_name = tracekey.at("name", "tracekey").as_string();
        gutl::UTL_StringToUpperCase(my_data->trace_name);

        my_data->trace_unit = tracekey.at("unit", "tracekey").as_string();

        my_data->tmin = tracekey.at("tmin", "tracekey").as_float();
        my_data->tmax = tracekey.at("tmax", "tracekey").as_float();
        my_data->trace_length = tracekey.at("length", "tracekey").as_int();

        gd_logger.LogInfo(my_logger, "Data Axis: {}, Length: {}, [{} -- {}] "
            , my_data->trace_name, my_data->trace_length, my_data->tmin, my_data->tmax);

        int grp_size = job_df.GetGroupSize();

        my_data->group_size = grp_size;
        std::string pattern = tracekey.at("pattern", "tracekey").as_string();
        gutl::UTL_StringToUpperCase(pattern);

        


        my_data->attrs.emplace_back(my_data->trace_name, my_data->trace_unit
            , my_data->trace_length, as::DataFormat::FORMAT_R32
            , to_checkpattern(pattern)); 

        //parse attributes
        auto& attrs = config["testexpect"]["attribute"];
        if(attrs.is_array()) {
            auto& arr = attrs.as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                //AttrConfig attr_config;
                std::string name = arr[i].at("name", "attribute").as_string();
                gutl::UTL_StringToUpperCase(name);

                std::string pattern = arr[i].at("pattern", "attribute").as_string();
                gutl::UTL_StringToUpperCase(pattern);

                my_data->attrs.emplace_back(name, arr[i].at("unit", "attribute").as_string()
                    , arr[i].at("length", "attribute").as_int()
                    , as::string_to_data_format(arr[i].at("type", "attribute").as_string())
                    , to_checkpattern(pattern)); 
            } 
        }
           

        //check axis
        int pmin, pmax, pnums;
        int smin, smax, snums;
        float tmin, tmax;
        int tnums;

        job_df.GetPrimaryKeyAxis(pmin, pmax, pnums);
        job_df.GetSecondaryKeyAxis(smin, smax, snums);
        job_df.GetDataAxis(tmin, tmax, tnums);

        gd_logger.LogInfo(my_logger, "primary {} {} {}", pmin, pmax, pnums);
        gd_logger.LogInfo(my_logger, "secondary {} {} {}", smin, smax, snums);
        gd_logger.LogInfo(my_logger, "trace {} {} {}", tmin, tmax, tnums);

        if(my_data->fpkey != pmin) {
            throw std::runtime_error("Primary min error, expect = " + std::to_string(pmin) + " but got " + std::to_string(my_data->fpkey));
        }

        if(my_data->lpkey != pmax) {
            throw std::runtime_error("Primary max error, expect = " + std::to_string(pmax) + " but got " + std::to_string(my_data->lpkey)); 
        }

        std::string pname(job_df.GetPrimaryKeyName());

        if(my_data->pkey_name != pname) {
            throw std::runtime_error("Primary name error, expect = " + pname + " but got " + my_data->pkey_name); 
        }        

        if(my_data->fskey != smin) {
            throw std::runtime_error("Secondary min error, expect = " + std::to_string(smin) + " but got " + std::to_string(my_data->fskey));
        }

        if(my_data->lskey != smax) {
            throw std::runtime_error("Secondary max error, expect = " + std::to_string(smax) + " but got " + std::to_string(my_data->lskey));            
        }

        std::string sname(job_df.GetSecondaryKeyName());

        if(my_data->skey_name != sname) {
            throw std::runtime_error("Secondary name error, expect = " + sname + " but got " + my_data->skey_name); 
        }   

        if(my_data->tmin != tmin) {
            throw std::runtime_error("Trace min error, expect = " + std::to_string(tmin) + " but got " + std::to_string(my_data->tmin));            

        }

        if(my_data->tmax != tmax) {
            throw std::runtime_error("Trace max error, expect = " + std::to_string(tmax) + " but got " + std::to_string(my_data->tmax));            
        }      

        if(my_data->trace_length != tnums) {
            throw std::runtime_error("Trace length error, expect = " + std::to_string(tnums) + " but got " + std::to_string(my_data->trace_length));            
        }
      
        std::string tname(job_df.GetVolumeDataName());

        if(my_data->trace_name != tname) {
            throw std::runtime_error("Trace name error, expect = " + tname + " but got " + my_data->trace_name); 
        }  
        
        gd_logger.LogDebug(my_logger, "attr num {}", job_df.GetNumAttributes());
        gd_logger.LogDebug(my_logger, "attr num {}", my_data->attrs.size());


        //check attributes
        std::string primary_name = job_df.GetPrimaryKeyName();
        std::string secondary_name = job_df.GetSecondaryKeyName();

        for(int j = 0; j< job_df.GetNumAttributes(); ++j) {
            as::DataFormat attr_fmt;
            int length;
            float min; 
            float max;
            std::string attr_name = job_df.GetAttributeName(j);
            if(attr_name == primary_name || attr_name == secondary_name)
                continue;
            job_df.GetAttributeInfo(attr_name.c_str(), attr_fmt, length, min, max);
            int i;
            for(i = 0; i < my_data->attrs.size(); ++i) {
                if(my_data->attrs[i].name == attr_name)
                    break;
            }

            if(i >= my_data->attrs.size()) {
                throw std::runtime_error("Attributes [" + attr_name + "] cannot found."); 
            }

            gd_logger.LogDebug(my_logger, "attr {} {} {} {} ", i, j, my_data->attrs[i].name, attr_name);
            gd_logger.LogDebug(my_logger, "attr {} {} {} {} ", i, j, std::to_string(static_cast<int>(my_data->attrs[i].type)), std::to_string(static_cast<int>(attr_fmt)));
            gd_logger.LogDebug(my_logger, "attr {} {} {} {} ", i, j, std::to_string(my_data->attrs[i].length), length);
            
            if(my_data->attrs[i].name != attr_name) {
                throw std::runtime_error("Attr [" + std::to_string(i) + "] name error, expect = " + my_data->attrs[i].name + " but got " + attr_name);            
            }            

            if(my_data->attrs[i].type != attr_fmt) {
                throw std::runtime_error("Attr [" + std::to_string(i) + "] type error, expect = " + as::data_format_to_string(my_data->attrs[i].type) + " but got " + as::data_format_to_string(attr_fmt));            
            }              

            if(my_data->attrs[i].length != length) {
                throw std::runtime_error("Attr [" + std::to_string(i) + "] length error, expect = " + std::to_string(my_data->attrs[i].length) + " but got " + std::to_string(length));            
            }
        }

        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

        gd_logger.FlushLog(my_logger);

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
 
}

void testexpect_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Testexpect* my_data = static_cast<Testexpect*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    // A handy function to clean up resources if errors happen, 
    // or job has finished
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    if (job_df.JobFinished()) {
        _clean_up();
        return;
    }

    // have we reached the end of data
    if (my_data->pkinc > 0) {
        if (my_data->current_pkey > my_data->lpkey) {
        // set the flag
        job_df.SetJobFinished();
        return;
        }
    }
    else {
        if (my_data->current_pkey < my_data->lpkey) {
        // set the flag
        job_df.SetJobFinished();
        return;
        }
    }

    try {

        int* pkey;
        pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
        if(pkey == nullptr) {
            throw std::runtime_error("DF returned a nullptr to the buffer of pkey is NULL");
        }

        int grp_size = job_df.GetGroupSize();

        // setup attributes
        std::map<std::string, AttrData> variables;

        const char* attr_name;
        int length;
        as::DataFormat attr_fmt;
        float min;
        float max;
        void* data;

        std::string primary_name = job_df.GetPrimaryKeyName();

        std::string secondary_name = job_df.GetSecondaryKeyName();
        
        //get primary and secondary
        for(int i = 0; i < job_df.GetNumAttributes(); i++) {

            std::string attr_name = job_df.GetAttributeName(i);

            if((attr_name != primary_name) &&(attr_name != secondary_name)) 
                continue;

            job_df.GetAttributeInfo(attr_name.c_str(), attr_fmt, length, min, max);
            data = job_df.GetWritableBuffer(attr_name.c_str());
            variables.emplace(attr_name, AttrData(data, (size_t)length * grp_size, attr_fmt));
        }
        
        // load data
        for(int i = 0; i < my_data->attrs.size(); ++i) {
            if(my_data->attrs[i].check_pattern == CheckPattern::ATTR_PLUS_MUL)
                continue;
            load_data(my_data->attrs[i].data, my_data->attrs[i].name + ".DAT", grp_size * my_data->attrs[i].length * as::get_data_format_size(my_data->attrs[i].type));
            data = static_cast<void*>(my_data->attrs[i].data.data());
            variables.emplace(my_data->attrs[i].name, AttrData(data, (size_t)my_data->attrs[i].length * grp_size, my_data->attrs[i].type));
        }

        //check attributes
        for(int i = 0; i < my_data->attrs.size(); i++) {

            std::string attr_name = my_data->attrs[i].name;
            gd_logger.LogInfo(my_logger, "check data attributes {} {}", i, attr_name);

            job_df.GetAttributeInfo(attr_name.c_str(), attr_fmt, length, min, max);
            data = job_df.GetWritableBuffer(attr_name.c_str());
            AttrData attr_data(data, (size_t)length * grp_size, attr_fmt);

            if(check_data(my_data, attr_name, attr_data, variables, get_pattern(attr_name, my_data->attrs))) {
                gd_logger.LogInfo(my_logger, "Attribute [{}] check data success.", attr_name);
            } else {
                throw std::runtime_error("Attribute [" + attr_name + "]  check data failed.");
                //gd_logger.LogError(my_logger, "Attribute [{}] check data failed.", attr_name);
            }
        }
    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
    }

    // prepare for the next call
    my_data->current_pkey = my_data->current_pkey + my_data->pkinc;
}
