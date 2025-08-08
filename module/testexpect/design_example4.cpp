// 方案4：模板化的类型安全检查器
#include "testexpect.h"
#include <type_traits>
#include <array>

// 类型安全的数据访问器
template<typename T>
class TypedDataAccessor {
public:
    TypedDataAccessor(AttrData& attr_data, int group_size) 
        : data_(static_cast<T*>(attr_data.data)), 
          length_(attr_data.length * group_size) {
        if (!data_) {
            throw std::runtime_error("Data pointer is null");
        }
    }
    
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    size_t size() const { return length_; }
    T* data() const { return data_; }
    
private:
    T* data_;
    size_t length_;
};

// 模板化检查器基类
template<typename T>
class TypedChecker {
public:
    virtual ~TypedChecker() = default;
    virtual bool check(TypedDataAccessor<T>& data, 
                      std::map<std::string, AttrData>& variables,
                      Testexpect* context) = 0;
    virtual std::string name() const = 0;
};

// 具体的模板化检查器
template<typename T>
class RangeChecker : public TypedChecker<T> {
public:
    RangeChecker(T min_val, T max_val) : min_value_(min_val), max_value_(max_val) {}
    
    bool check(TypedDataAccessor<T>& data, 
              std::map<std::string, AttrData>& variables,
              Testexpect* context) override {
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] < min_value_ || data[i] > max_value_) {
                return false;
            }
        }
        return true;
    }
    
    std::string name() const override {
        return "Range[" + std::to_string(min_value_) + "," + std::to_string(max_value_) + "]";
    }
    
private:
    T min_value_, max_value_;
};

template<typename T>
class MonotonicChecker : public TypedChecker<T> {
public:
    enum Direction { INCREASING, DECREASING, NON_DECREASING, NON_INCREASING };
    
    MonotonicChecker(Direction dir) : direction_(dir) {}
    
    bool check(TypedDataAccessor<T>& data, 
              std::map<std::string, AttrData>& variables,
              Testexpect* context) override {
        if (data.size() < 2) return true;
        
        for (size_t i = 1; i < data.size(); ++i) {
            switch (direction_) {
                case INCREASING:
                    if (data[i] <= data[i-1]) return false;
                    break;
                case DECREASING:
                    if (data[i] >= data[i-1]) return false;
                    break;
                case NON_DECREASING:
                    if (data[i] < data[i-1]) return false;
                    break;
                case NON_INCREASING:
                    if (data[i] > data[i-1]) return false;
                    break;
            }
        }
        return true;
    }
    
    std::string name() const override {
        static const char* names[] = {"Increasing", "Decreasing", "NonDecreasing", "NonIncreasing"};
        return names[direction_];
    }
    
private:
    Direction direction_;
};

// 统计检查器
template<typename T>
class StatisticsChecker : public TypedChecker<T> {
public:
    struct Stats {
        double mean, variance, std_dev, min_val, max_val;
    };
    
    StatisticsChecker(const Stats& expected, double tolerance = 1e-6) 
        : expected_(expected), tolerance_(tolerance) {}
    
    bool check(TypedDataAccessor<T>& data, 
              std::map<std::string, AttrData>& variables,
              Testexpect* context) override {
        Stats actual = calculateStats(data);
        
        return (std::abs(actual.mean - expected_.mean) <= tolerance_ &&
                std::abs(actual.std_dev - expected_.std_dev) <= tolerance_);
    }
    
    std::string name() const override {
        return "Statistics[mean=" + std::to_string(expected_.mean) + 
               ",std=" + std::to_string(expected_.std_dev) + "]";
    }
    
private:
    Stats expected_;
    double tolerance_;
    
    Stats calculateStats(TypedDataAccessor<T>& data) {
        Stats stats = {};
        if (data.size() == 0) return stats;
        
        double sum = 0, sum_sq = 0;
        stats.min_val = stats.max_val = static_cast<double>(data[0]);
        
        for (size_t i = 0; i < data.size(); ++i) {
            double val = static_cast<double>(data[i]);
            sum += val;
            sum_sq += val * val;
            stats.min_val = std::min(stats.min_val, val);
            stats.max_val = std::max(stats.max_val, val);
        }
        
        stats.mean = sum / data.size();
        stats.variance = sum_sq / data.size() - stats.mean * stats.mean;
        stats.std_dev = std::sqrt(stats.variance);
        
        return stats;
    }
};

// 类型分发器
class TypeDispatcher {
public:
    template<typename CheckerType>
    static bool dispatch(as::DataFormat format, AttrData& attr_data, int group_size,
                        std::map<std::string, AttrData>& variables, Testexpect* context,
                        CheckerType&& checker_factory) {
        switch (format) {
            case as::DataFormat::FORMAT_R32: {
                TypedDataAccessor<float> accessor(attr_data, group_size);
                auto checker = checker_factory.template create<float>();
                return checker->check(accessor, variables, context);
            }
            case as::DataFormat::FORMAT_R64: {
                TypedDataAccessor<double> accessor(attr_data, group_size);
                auto checker = checker_factory.template create<double>();
                return checker->check(accessor, variables, context);
            }
            case as::DataFormat::FORMAT_U32: {
                TypedDataAccessor<uint32_t> accessor(attr_data, group_size);
                auto checker = checker_factory.template create<uint32_t>();
                return checker->check(accessor, variables, context);
            }
            // 添加其他类型...
            default:
                throw std::runtime_error("Unsupported data format");
        }
    }
};

// 检查器工厂
struct RangeCheckerFactory {
    double min_val, max_val;
    
    template<typename T>
    std::unique_ptr<TypedChecker<T>> create() {
        return std::make_unique<RangeChecker<T>>(static_cast<T>(min_val), static_cast<T>(max_val));
    }
};

// 使用示例
bool check_data_typed(Testexpect* my_data, const std::string& attr_name, 
                     AttrData& attr_data, std::map<std::string, AttrData>& variables,
                     const std::string& pattern_name) {
    
    if (pattern_name.find("RANGE[") == 0) {
        // 解析范围参数
        // 简化版本，实际需要更好的解析
        RangeCheckerFactory factory{0.0, 100.0};
        return TypeDispatcher::dispatch(attr_data.type, attr_data, my_data->group_size,
                                       variables, my_data, factory);
    }
    
    // 其他模式...
    return false;
}