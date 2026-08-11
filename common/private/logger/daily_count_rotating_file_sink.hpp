#ifndef COREXI_COMMON_PC_DAILY_COUNT_ROTATING_FILE_SINK_H
#define COREXI_COMMON_PC_DAILY_COUNT_ROTATING_FILE_SINK_H
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <string>
#include <vector>

namespace CustomSink
{
    namespace fs = std::filesystem;

    static bool ends_with(const std::string& s, const std::string& suffix)
    {
        if (s.size() < suffix.size()) return false;
        return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
    }

    static bool starts_with(const std::string &s, const std::string &prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    // 取本地日期：YYYY-MM-DD
    static std::string local_date_yyyy_mm_dd()
    {
        std::time_t t = std::time(nullptr);
        std::tm tmv{};
    #if defined(_WIN32)
        localtime_s(&tmv, &t);
    #else
        localtime_r(&t, &tmv);
    #endif
        char buf[11];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                      tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
        return std::string(buf);
    }

    template<typename Mutex>
    class daily_size_rotating_file_mt : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        daily_size_rotating_file_mt(const std::string& base_filename,
                                    size_t max_size_bytes,
                                    size_t max_files = 0,
                                    bool rotate_on_open = false)
            : max_size_(max_size_bytes)
            , max_files_(max_files)
            , rotate_on_open_(rotate_on_open)
        {
            // -------------------------------
            // 拆分目录、stem、扩展名（默认 .log）
            // base_filename 允许：
            //   "logs/app.log" 或 "logs/app"
            // -------------------------------
            fs::path p(base_filename);

            dir_ = p.has_parent_path() ? p.parent_path() : fs::path(".");
            std::string filename = p.filename().string();

            if (ends_with(filename, ".log"))
            {
                stem_ = filename.substr(0, filename.size() - 4);
            }
            else
            {
                stem_ = filename;
            }
            extension_ = ".log";

            current_date_ = local_date_yyyy_mm_dd();

            // 确保目录存在
            std::error_code ec;
            fs::create_directories(dir_, ec);

            // 扫描当天已有文件
            scan_existing_files_for_today_();

            if (rotate_on_open_ && !files_.empty())
            {
                open_new_file_();     // 直接开新卷
            }
            else
            {
                if (files_.empty())
                {
                    current_index_ = 0;
                }
                else
                {
                    current_index_ = extract_index_(files_.back());
                }
                open_current_file_();
            }
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            maybe_roll_day_();

            spdlog::memory_buf_t buf;
            this->formatter_->format(msg, buf);

            // 去掉尾部换行符（避免重复换行）
            size_t size = buf.size();
            while (size > 0 && (buf[size - 1] == '\n' || buf[size - 1] == '\r'))
            {
                --size;
            }

            // 如果单条就已经 > max_size_，也照写；写完再触发 rotate
            file_.write(buf.data(), static_cast<std::streamsize>(size));
            file_.put('\n');

            current_size_ += size + 1; // + '\n'

            if (max_size_ > 0 && current_size_ >= max_size_)
            {
                rotate_file_();
            }
        }

        void flush_() override
        {
            file_.flush();
        }

    private:
        //-----------------------------------------------------
        // 当天文件结构：
        //   stem_YYYY-MM-DD.log      -> index 0
        //   stem_YYYY-MM-DD.1.log    -> index 1
        //   stem_YYYY-MM-DD.2.log    -> index 2
        //-----------------------------------------------------
        std::string today_prefix_() const
        {
            // stem_YYYY-MM-DD
            return stem_ + "_" + current_date_;
        }

        std::string make_filename_(size_t index) const
        {
            const std::string prefix = today_prefix_();
            if (index == 0)
                return prefix + extension_;
            return prefix + "." + std::to_string(index) + extension_;
        }

        fs::path make_path_(size_t index) const
        {
            return dir_ / fs::path(make_filename_(index));
        }

        void scan_existing_files_for_today_()
        {
            files_.clear();

            const std::string prefix = today_prefix_();

            std::error_code ec;
            if (!fs::exists(dir_, ec) || !fs::is_directory(dir_, ec))
                return;

            for (auto& it : fs::directory_iterator(dir_, ec))
            {
                if (ec) break;
                if (!it.is_regular_file(ec)) continue;

                const std::string name = it.path().filename().string();

                if (name == prefix + extension_)
                {
                    files_.push_back(name);
                }
                else if (starts_with(name, prefix + ".") && ends_with(name, extension_))
                {
                    files_.push_back(name);
                }
            }

            std::sort(files_.begin(), files_.end(),
                      [&](const std::string& a, const std::string& b) {
                          return extract_index_(a) < extract_index_(b);
                      });
        }

        size_t extract_index_(const std::string& filename) const
        {
            const std::string prefix = today_prefix_();
            const std::string first  = prefix + extension_;
            if (filename == first) return 0;

            std::string pfx = prefix + ".";
            if (starts_with(filename, pfx) && ends_with(filename, extension_))
            {
                size_t start = pfx.size();
                size_t end   = filename.size() - extension_.size();
                std::string middle = filename.substr(start, end - start);
                return static_cast<size_t>(std::stoull(middle));
            }
            return 0;
        }

        void open_current_file_()
        {
            // 关闭旧文件（保险）
            if (file_.is_open()) file_.close();

            const fs::path path = make_path_(current_index_);
            file_.open(path.string(), std::ios::out | std::ios::app);

            // 当前大小 = 已有文件大小（append 续写时重要）
            current_size_ = 0;
            std::error_code ec;
            if (fs::exists(path, ec))
            {
                auto sz = fs::file_size(path, ec);
                if (!ec) current_size_ = static_cast<size_t>(sz);
            }

            const std::string name = path.filename().string();
            if (files_.empty() || files_.back() != name)
                files_.push_back(name);
        }

        void open_new_file_()
        {
            current_index_++;
            const fs::path path = make_path_(current_index_);
            file_.open(path.string(), std::ios::out | std::ios::app);

            current_size_ = 0;
            std::error_code ec;
            if (fs::exists(path, ec))
            {
                auto sz = fs::file_size(path, ec);
                if (!ec) current_size_ = static_cast<size_t>(sz);
            }

            files_.push_back(path.filename().string());
            cleanup_old_files_();
        }

        void rotate_file_()
        {
            file_.flush();
            file_.close();
            open_new_file_();
        }

        void cleanup_old_files_()
        {
            if (max_files_ == 0) return;

            // max_files_：当天最多保留多少个文件（含 index 0）
            while (files_.size() > max_files_)
            {
                const fs::path path = dir_ / fs::path(files_.front());
                std::error_code ec;
                fs::remove(path, ec);
                files_.erase(files_.begin());
            }
        }

        void maybe_roll_day_()
        {
            const std::string today = local_date_yyyy_mm_dd();
            if (today == current_date_)
                return;

            // 跨天：flush + close + 重置（只影响当天状态）
            file_.flush();
            file_.close();

            current_date_ = today;
            current_index_ = 0;
            current_size_ = 0;

            std::error_code ec;
            fs::create_directories(dir_, ec);

            scan_existing_files_for_today_();

            if (rotate_on_open_ && !files_.empty())
            {
                open_new_file_();
            }
            else
            {
                if (files_.empty())
                    current_index_ = 0;
                else
                    current_index_ = extract_index_(files_.back());

                open_current_file_();
            }
        }

    private:
        std::ofstream file_;

        fs::path dir_;
        std::string stem_;
        std::string extension_;

        size_t max_size_;
        size_t max_files_;
        bool rotate_on_open_;

        std::string current_date_;
        size_t current_index_ = 0;
        size_t current_size_  = 0;

        std::vector<std::string> files_;
    };

} // namespace CustomSink
#endif//COREXI_COMMON_PC_DAILY_COUNT_ROTATING_FILE_SINK_H
