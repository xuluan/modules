
#include "testexpect.h"
#include "attrcalc_expect.h"
#include <GdLogger.h>



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