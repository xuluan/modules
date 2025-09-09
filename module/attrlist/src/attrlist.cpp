#include "attrlist.h"
#include "ArrowStore.h"
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


std::string get_data_type_to_string(as::DataFormat format) {
    switch (format) {
        case as::DataFormat::FORMAT_U8:
            return "int8";
        case as::DataFormat::FORMAT_U16:
            return "int16";
        case as::DataFormat::FORMAT_R32:
            return "float";
        case as::DataFormat::FORMAT_U32:
            return "int32";
        case as::DataFormat::FORMAT_R64:
            return "double";
        case as::DataFormat::FORMAT_U64:
            return "int64";
        default:
            throw std::runtime_error("No support value for as::DataFormat : ");
    }
}

void attrlist_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"attrlist_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "attrlist_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Attrlist* my_data = new Attrlist {};

    my_data->logger = my_logger;

    auto& job_df = df::GeoDataFlow::GetInstance();

    // A handy function to clean up resources if errors happen
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    try {

        //check 
        int pmin, pmax, pnums;
        int smin, smax, snums;
        float tmin, tmax;
        int tnums;

        // parse job parameters
        gutl::DynamicValue config = gutl::parse(buf);

        job_df.GetPrimaryKeyAxis(pmin, pmax, pnums);
        job_df.GetSecondaryKeyAxis(smin, smax, snums);
        job_df.GetDataAxis(tmin, tmax, tnums);

        gd_logger.LogInfo(my_logger, "Primary Axis: {}, [{} -- {}], nums: {} "
            , job_df.GetPrimaryKeyName(), pmin, pmax, pnums);
       
        gd_logger.LogInfo(my_logger, "Secondary Axis: {}, [{} -- {}], nums: {} "
            , job_df.GetSecondaryKeyName(), smin, smax, snums);            

        gd_logger.LogInfo(my_logger, "Data Axis: {}, [{} -- {}], nums: {} "
            , job_df.GetVolumeDataName(), tmin, tmax, tnums);   


        int grp_size = job_df.GetGroupSize();

        gd_logger.LogInfo(my_logger, "Attribute Group size {}", grp_size);

        //parse attributes
        auto& attrs = config["attrlist"]["attributes"];

        if (attrs.is_array()) {
            gd_logger.LogInfo(my_logger, "Attributes list:");
            auto& arr = attrs.as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                as::DataFormat attr_fmt;
                int length;
                float min; 
                float max;
                //AttrConfig attr_config;
                std::string attr_name = arr[i].as_string();
                gutl::UTL_StringToUpperCase(attr_name);

                if (job_df.HasAttribute(attr_name.c_str()) ) {
                    job_df.GetAttributeInfo(attr_name.c_str(), attr_fmt, length, min, max);

                    gd_logger.LogInfo(my_logger, "Attribute {:2}, Name: {:32}, Type: {:6}, Length: {:10}, Min: {:10}, Max: {:10}"
                        , i, attr_name, get_data_type_to_string(attr_fmt), length, min, max);

                } else {
                    gd_logger.LogWarning(my_logger, "Attribute {:2}, Name: {:32}  cannot found"
                        , i, attr_name);
                    throw std::runtime_error("Attribute [" + attr_name + "] cannot found."); 
                }

            } 
        } else {
            // list all attribute if 'attribute' is miss
            gd_logger.LogInfo(my_logger, "All Attributes:");

            for(int i = 0; i< job_df.GetNumAttributes(); i++) {
                as::DataFormat attr_fmt;
                int length;
                float min; 
                float max;
                const char * attr_name = job_df.GetAttributeName(i);
                job_df.GetAttributeInfo(attr_name, attr_fmt, length, min, max);

                gd_logger.LogInfo(my_logger, "Attribute {:2}, Name: {:32}, Type: {:6}, Length: {:10}, Min: {:10}, Max: {:10}"
                    , i, attr_name, get_data_type_to_string(attr_fmt), length, min, max); 
            }

        }

        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

        gd_logger.FlushLog(my_logger);

    } catch (const std::exception& e) {
        std::cerr << "Failed: " << e.what() << std::endl;
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
 
}

void attrlist_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Attrlist* my_data = static_cast<Attrlist*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    gd_logger.LogInfo(my_logger, "attrlist_process begin");

    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    if (job_df.JobFinished()) {
        _clean_up();
        return;
    }
}
