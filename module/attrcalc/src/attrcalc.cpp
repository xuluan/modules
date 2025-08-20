#include "attrcalc.h"

#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <utl_string.h>

bool validAttrName(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    
    char firstChar = str[0];
    
    return (std::isalpha(firstChar) != 0) || (firstChar == '_');
}

void attrcalc_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"attrcalc_"} + myid;
    std::string attr_data_type;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "attrcalc_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Attrcalc* my_data = new Attrcalc {};

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
        mc::ModuleConfig mod_conf {};  
        mod_conf.Parse(buf);
        mod_conf.GetText("attrcalc.attrname", my_data->name);
        if(mod_conf.HasError()) {
            throw std::runtime_error("Failed to get attrcalc attrname. Error: " + mod_conf.ErrorMessage());
        }
        gutl::UTL_StringToUpperCase(my_data->name);

        mod_conf.GetText("attrcalc.action", my_data->action);
        if(mod_conf.HasError()) {
            throw std::runtime_error("Failed to get attrcalc action. Error: " + mod_conf.ErrorMessage());
        }

        mod_conf.GetText("attrcalc.expr", my_data->expr);
        if(mod_conf.HasError()) {
            throw std::runtime_error("Failed to get attrcalc expr. Error: " + mod_conf.ErrorMessage());
        }
        gutl::UTL_StringToUpperCase(my_data->expr);

        mod_conf.GetText("attrcalc.type", my_data->type);
        if(mod_conf.HasError()) {
            throw std::runtime_error("Failed to get attrcalc datatype. Error: " + mod_conf.ErrorMessage());
        }

        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));
        
        // setup attributes
        std::vector<std::string> variables;
        std::map<std::string, int> vars_map;

        const char* attr_name;
        int length;
        as::DataFormat attr_fmt;
        float min;
        float max;

        for(int i = 0; i< job_df.GetNumAttributes(); i++) {
            attr_name = job_df.GetAttributeName(i);
            job_df.GetAttributeInfo(attr_name, attr_fmt, length, min, max);
            variables.push_back(attr_name);
            vars_map[attr_name] = length;
        }

        //action
        if(my_data->action == "remove") {
            auto it = std::find(variables.begin(), variables.end(), my_data->name);
            if(it == variables.end()) {
                throw std::runtime_error("Failed to find the attr to remove: " + my_data->name);
            }
            job_df.DeleteAttribute(my_data->name.c_str());
            return;
        } else if (my_data->action == "update") {
            auto it = std::find(variables.begin(), variables.end(), my_data->name);
            if(it == variables.end()) {
                throw std::runtime_error("Failed to find the attr to update: " + my_data->name);
            }
        } else if (my_data->action == "create") {
            auto it = std::find(variables.begin(), variables.end(), my_data->name);
            if(it != variables.end()) {
                throw std::runtime_error("Attr Name exists: " + my_data->name);
            }            
            if (!validAttrName(my_data->name)) {
                throw std::runtime_error("Attr Name is invalid. It must start with a letter or an underscore: " + my_data->name);
            }
            variables.push_back(my_data->name);
            my_data->attr_data.type = as::string_to_data_format(my_data->type);
        } else {
            throw std::runtime_error("Action is invalid: " + my_data->action);
        }


        //parser expr
        ExpressionParser parser;
        bool success = parser.parse(my_data->expr, variables, my_data->expression);
        if(!success) {
            throw std::runtime_error(parser.get_errors());
        }
        
        int prev = -1;
        std::string prev_name;
    
        for (const std::string& str : parser.get_used_variables()) {
            if(prev == -1) {
                prev_name = str;
                prev = vars_map[str];
            } else {
                if(prev != vars_map[str]) {
                    throw std::runtime_error("Attribute lengths should be the same, but " 
                        + prev_name + " = " + std::to_string(prev) + " vs " + str + " = " + std::to_string(vars_map[str]));                    
                } 
            }
        }

        if (my_data->action == "create") {
            my_data->attr_data.length = (size_t)prev;
            job_df.AddAttribute(my_data->name.c_str(), my_data->attr_data.type, my_data->attr_data.length);
        }

        gd_logger.LogInfo(my_logger, "Attr name: {}, datatype: {}, action: {}, expr: {} ", my_data->name, my_data->type, my_data->action, my_data->expr);

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
    }

}

void attrcalc_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Attrcalc* my_data = static_cast<Attrcalc*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    gd_logger.LogInfo(my_logger, "attrcalc_process begin");

    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    if (job_df.JobFinished()) {
        _clean_up();
        return;
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
        AttrData attr_data;

        for(int i = 0; i< job_df.GetNumAttributes(); i++) {
            attr_name = job_df.GetAttributeName(i);
            job_df.GetAttributeInfo(attr_name, attr_fmt, length, min, max);
            data = job_df.GetWritableBuffer(attr_name);
            attr_data = {data, (size_t)length * grp_size, attr_fmt};
            variables[attr_name] = attr_data;
        }

        std::vector<double> result_data;
        result_data.resize(length * grp_size);
        AttrData result_attr = {result_data.data(), (size_t)length * grp_size, as::DataFormat::FORMAT_R64};

        ExpressionEvaluator evaluator;
        bool success = evaluator.evaluate(my_data->expression, variables, &result_attr);
        if(!success) {
            throw std::runtime_error(evaluator.get_errors());
        }
        my_data->attr_data = variables[my_data->name];

        convert_vector(&my_data->attr_data, &result_attr);

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
    }

}
