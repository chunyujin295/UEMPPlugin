/*************************************************
 * 描述：按日期分割 + 单日按大小滚动的 sink（无 Qt 依赖）
 *
 * 文件结构（每天一个“组”，组内滚动）：
 *   <pattern_with_date>.log          // 当天当前写入
 *   <pattern_with_date>.1.log        // 当天第1个备份（最新旧文件）
 *   ...
 *   <pattern_with_date>.N.log        // 当天第N个备份（最老）
 *
 * pattern 规则：
 *   - name_pattern 中用 "{date}" 占位，其余为自定义字段，可在前可在后
 *   - date_format 使用 Qt 风格子集（yyyy/MM/dd 等），由本文件手写解析器实现
 *
 * 关键语义（与你要求对齐）：
 *   - “无后缀 .log”永远代表当前文件，程序结束时也应该存在
 *   - size 滚动仅在“写入前”触发；写完后不滚动
 *   - max_files == 0：禁用 size 滚动，永远追加写当天 base 文件，不清空
 ************************************************/
#ifndef COREXI_COMMON_PC_DAILY_SIZE_ROTATING_SINK_NOQT_HPP
#define COREXI_COMMON_PC_DAILY_SIZE_ROTATING_SINK_NOQT_HPP

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <spdlog/details/file_helper.h>
#include <spdlog/sinks/base_sink.h>

namespace CustomSink
{
namespace fs = std::filesystem;

// ---------------- Qt-like datetime formatter (subset) ----------------
static inline void append_int_(std::string& out, int v, int width, char pad = '0')
{
    std::ostringstream oss;
    if (width > 0) oss << std::setw(width) << std::setfill(pad);
    oss << v;
    out += oss.str();
}

static inline int clamp_ms_(int ms)
{
    if (ms < 0) return 0;
    if (ms > 999) return 999;
    return ms;
}

struct LocalDateTimeParts_
{
    std::tm tm{};
    int millisecond = 0;
};

static inline LocalDateTimeParts_ to_local_parts_(std::chrono::system_clock::time_point tp)
{
    using namespace std::chrono;
    const auto ms_since_epoch = duration_cast<milliseconds>(tp.time_since_epoch());
    const auto sec_since_epoch = duration_cast<seconds>(ms_since_epoch);
    const int ms = (int)(ms_since_epoch - sec_since_epoch).count();

    std::time_t tt = std::chrono::system_clock::to_time_t(tp);

    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &tt);
#else
    localtime_r(&tt, &local);
#endif

    LocalDateTimeParts_ p;
    p.tm = local;
    p.millisecond = clamp_ms_(ms);
    return p;
}

// 支持 token：yyyy/yy, MM/M, dd/d, HH/H, hh/h, mm/m, ss/s, z/zz/zzz, AP/ap, MMM/MMMM, ddd/dddd
static inline std::string qt_datetime_format_subset_(std::chrono::system_clock::time_point tp,
                                                     std::string_view fmt)
{
    static constexpr std::array<const char*, 12> kMonthShort{
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    static constexpr std::array<const char*, 12> kMonthLong{
        "January","February","March","April","May","June",
        "July","August","September","October","November","December"};
    static constexpr std::array<const char*, 7> kWeekdayShort{
        "Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    static constexpr std::array<const char*, 7> kWeekdayLong{
        "Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};

    const auto parts = to_local_parts_(tp);
    const std::tm& t = parts.tm;

    const int year   = t.tm_year + 1900;
    const int month  = t.tm_mon + 1;
    const int day    = t.tm_mday;
    const int wday   = t.tm_wday;   // 0=Sun..6=Sat
    const int hour24 = t.tm_hour;
    const int minute = t.tm_min;
    const int second = t.tm_sec;

    const int qtWdayIndex = (wday == 0) ? 6 : (wday - 1);

    auto hour12 = [&] {
        int h = hour24 % 12;
        return (h == 0) ? 12 : h;
    };

    auto ampm = [&](bool upper) -> const char* {
        const bool is_pm = (hour24 >= 12);
        if (upper) return is_pm ? "PM" : "AM";
        return is_pm ? "pm" : "am";
    };

    std::string out;
    out.reserve(fmt.size() + 32);

    bool in_quote = false;

    for (size_t i = 0; i < fmt.size(); )
    {
        char c = fmt[i];

        // Qt single quote literal: 'text', '' => '
        if (c == '\'')
        {
            if (i + 1 < fmt.size() && fmt[i + 1] == '\'')
            {
                out.push_back('\'');
                i += 2;
                continue;
            }
            in_quote = !in_quote;
            ++i;
            continue;
        }

        if (in_quote)
        {
            out.push_back(c);
            ++i;
            continue;
        }

        // AP / ap
        if (i + 1 < fmt.size() && ((fmt[i] == 'A' && fmt[i + 1] == 'P') ||
                                  (fmt[i] == 'a' && fmt[i + 1] == 'p')))
        {
            out += ampm(fmt[i] == 'A');
            i += 2;
            continue;
        }

        // count repeats
        size_t j = i + 1;
        while (j < fmt.size() && fmt[j] == c) ++j;
        const size_t n = j - i;

        switch (c)
        {
            case 'y':
                if (n >= 4) append_int_(out, year, 4);
                else if (n == 2) append_int_(out, year % 100, 2);
                else append_int_(out, year, 0);
                break;

            case 'M':
                if (n == 1) append_int_(out, month, 0);
                else if (n == 2) append_int_(out, month, 2);
                else if (n == 3) out += kMonthShort[(size_t)(month - 1)];
                else out += kMonthLong[(size_t)(month - 1)];
                break;

            case 'd':
                if (n == 1) append_int_(out, day, 0);
                else if (n == 2) append_int_(out, day, 2);
                else if (n == 3) out += kWeekdayShort[(size_t)qtWdayIndex];
                else out += kWeekdayLong[(size_t)qtWdayIndex];
                break;

            case 'H':
                if (n == 1) append_int_(out, hour24, 0);
                else append_int_(out, hour24, 2);
                break;

            case 'h':
            {
                int h12 = hour12();
                if (n == 1) append_int_(out, h12, 0);
                else append_int_(out, h12, 2);
                break;
            }

            case 'm':
                if (n == 1) append_int_(out, minute, 0);
                else append_int_(out, minute, 2);
                break;

            case 's':
                if (n == 1) append_int_(out, second, 0);
                else append_int_(out, second, 2);
                break;

            case 'z':
            {
                int ms = parts.millisecond;
                if (n == 1) append_int_(out, ms / 100, 0);
                else if (n == 2) append_int_(out, ms / 10, 2);
                else append_int_(out, ms, 3);
                break;
            }

            default:
                out.append(n, c); // unknown token treated as literal
                break;
        }

        i = j;
    }

    return out;
}

static inline std::string replace_all_(std::string s, std::string_view from, std::string_view to)
{
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from.data(), pos, from.size())) != std::string::npos)
    {
        s.replace(pos, from.size(), to.data(), to.size());
        pos += to.size();
    }
    return s;
}

// ---------------- Sink: daily + size rotating ----------------
template<typename Mutex>
class daily_size_rotating_file_mt : public spdlog::sinks::base_sink<Mutex>
{
public:
    // max_size_bits: bit（内部转 bytes，向上取整）
    // max_files:
    //   - 0：禁用 size 滚动，永远追加写当天 base 文件
    //   - >=1：启用 .1..N 覆盖滚动链
    daily_size_rotating_file_mt(fs::path dir,
                                std::string name_pattern,
                                std::string date_format,
                                int rotation_hour,
                                int rotation_min,
                                uint64_t max_size_bits,
                                size_t max_files,
                                bool rotate_on_open = false)
        : dir_(std::move(dir))
        , name_pattern_(std::move(name_pattern))
        , date_format_(std::move(date_format))
        , rotation_hour_(rotation_hour)
        , rotation_min_(rotation_min)
        , max_files_(max_files)
        , rotate_on_open_(rotate_on_open)
    {
        if (rotation_hour_ < 0 || rotation_hour_ > 23 || rotation_min_ < 0 || rotation_min_ > 59)
            throw std::invalid_argument("rotation time invalid");

        max_size_bytes_ = (max_size_bits + 7) / 8;
        if (max_size_bytes_ == 0)
            throw std::invalid_argument("max_size_bits must be > 0");

        std::error_code ec;
        fs::create_directories(dir_, ec);

        opened_ = false;
        rotated_on_open_done_ = false;
        current_size_bytes_ = 0;

        update_targets_for_now_();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        (void)msg;

        // 1) 日切
        rotate_if_needed_by_time_();

        // 2) rotate_on_open：只在启用 size 滚动时有意义
        if (!rotated_on_open_done_)
        {
            if (rotate_on_open_ && max_files_ > 0)
            {
                std::error_code ec;
                auto p = current_base_path_();
                if (fs::exists(p, ec) && !ec)
                    rotate_by_size_chain_();
            }
            rotated_on_open_done_ = true;
        }

        // 3) 懒打开
        ensure_opened_for_write_();

        // 4) 格式化
        spdlog::memory_buf_t buf;
        this->formatter_->format(msg, buf);

        const uint64_t will_write = static_cast<uint64_t>(buf.size());

        // 5) 写前 size 滚动（预判式）：如果写入后会超出容量，先滚动
        if (max_files_ > 0 && (current_size_bytes_ + will_write > max_size_bytes_))
        {
            rotate_by_size_chain_();
            ensure_opened_for_write_();
        }

        file_helper_.write(buf);
        current_size_bytes_ += will_write;

        // 6) 关键：写后不滚动（保证无后缀当前文件始终存在）
    }

    void flush_() override
    {
        file_helper_.flush();
    }

private:
    static inline std::chrono::system_clock::time_point now_()
    {
        return std::chrono::system_clock::now();
    }

    std::string make_basename_for_tp_(std::chrono::system_clock::time_point tp) const
    {
        const std::string date = qt_datetime_format_subset_(tp, date_format_);
        return replace_all_(name_pattern_, "{date}", date);
    }

    fs::path current_base_path_() const
    {
        return dir_ / fs::path(current_basename_ + extension_);
    }

    fs::path rotated_path_(size_t index) const
    {
        return dir_ / fs::path(current_basename_ + "." + std::to_string(index) + extension_);
    }

    std::chrono::system_clock::time_point compute_next_rotation_(std::chrono::system_clock::time_point tp_now) const
    {
        std::time_t tt = std::chrono::system_clock::to_time_t(tp_now);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &tt);
#else
        localtime_r(&tt, &local);
#endif

        std::tm rot_tm = local;
        rot_tm.tm_hour = rotation_hour_;
        rot_tm.tm_min  = rotation_min_;
        rot_tm.tm_sec  = 0;

        std::time_t rot_today = std::mktime(&rot_tm);
        auto tp_rot_today = std::chrono::system_clock::from_time_t(rot_today);

        if (tp_now < tp_rot_today)
            return tp_rot_today;

        return tp_rot_today + std::chrono::hours(24);
    }

    void update_targets_for_now_()
    {
        auto tp = now_();
        current_basename_ = make_basename_for_tp_(tp);
        next_rotation_ = compute_next_rotation_(tp);
    }

    void rotate_if_needed_by_time_()
    {
        auto tp = now_();
        if (tp < next_rotation_)
            return;

        // 到点日切：关闭当前文件，切换到新日期组
        close_file_();

        current_basename_ = make_basename_for_tp_(tp);

        // 防时间跳变/挂起：推进到未来
        next_rotation_ = compute_next_rotation_(tp);
        while (next_rotation_ <= tp)
            next_rotation_ += std::chrono::hours(24);

        // 新日期组第一次写允许 rotate_on_open 再生效一次（仅当 max_files_>0）
        rotated_on_open_done_ = false;
    }

    void ensure_opened_for_write_()
    {
        if (opened_)
            return;

        const fs::path p = current_base_path_();
        file_helper_.open(p.string());
        opened_ = true;

        current_size_bytes_ = static_cast<uint64_t>(file_helper_.size());
    }

    void close_file_()
    {
        file_helper_.close();
        opened_ = false;
        current_size_bytes_ = 0;
    }

    void rotate_by_size_chain_()
    {
        if (max_files_ == 0) return;

        close_file_();

        std::error_code ec;

        // 删除最老
        fs::remove(rotated_path_(max_files_), ec);

        // 依次后移：.(i-1) -> .i   (i = max_files ... 2)
        for (size_t i = max_files_; i > 1; --i)
        {
            fs::path src = rotated_path_(i - 1);
            fs::path dst = rotated_path_(i);

            ec.clear();
            if (fs::exists(src, ec) && !ec)
            {
                fs::remove(dst, ec);
                ec.clear();
                fs::rename(src, dst, ec);
            }
        }

        // base -> .1
        {
            fs::path src = current_base_path_();
            fs::path dst = rotated_path_(1);

            ec.clear();
            if (fs::exists(src, ec) && !ec)
            {
                fs::remove(dst, ec);
                ec.clear();
                fs::rename(src, dst, ec);
            }
        }
    }

private:
    spdlog::details::file_helper file_helper_;

    fs::path dir_;
    std::string name_pattern_;
    std::string date_format_;

    int rotation_hour_;
    int rotation_min_;

    uint64_t max_size_bytes_;
    size_t max_files_;
    bool rotate_on_open_;

    std::string current_basename_;
    const std::string extension_ = ".log";

    std::chrono::system_clock::time_point next_rotation_{};

    uint64_t current_size_bytes_ = 0;
    bool opened_ = false;
    bool rotated_on_open_done_ = false;
};

} // namespace CustomSink

#endif // COREXI_COMMON_PC_DAILY_SIZE_ROTATING_SINK_NOQT_HPP
