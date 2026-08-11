/*************************************************
  * 描述：按日志条数进行滚动的日志sink（spdlog式命名 + 覆盖滚动 + 懒创建）
  *
  * 文件结构（同spdlog rotating常见风格）：
  *   stem.log          // 当前写入
  *   stem.1.log        // 第1个备份（最新的旧文件）
  *   stem.2.log
  *   ...
  *   stem.N.log        // 第N个备份（最老）
  *
  * 特性：
  *  - 懒创建：构造时不创建空文件，首次写入才创建/打开 stem.log
  *  - rotate_on_open=true：首次写入前若 stem.log 已存在，则先做一次滚动（rename链条），再写新的 stem.log
  *  - strict_count_on_open=true：首次打开文件时统计已有行数，严格保证每个文件总行数 <= max_count
  *
  * 注意：
  *  - strict_count_on_open 只在“打开文件”时扫描行数，不会每条日志都扫
  ************************************************/
#ifndef COREXI_COMMON_PC_COUNT_ROTATING_SPDLOG_STYLE_SINK_HPP
#define COREXI_COMMON_PC_COUNT_ROTATING_SPDLOG_STYLE_SINK_HPP

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/details/file_helper.h>
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

	static bool starts_with(const std::string& s, const std::string& prefix)
	{
		return s.rfind(prefix, 0) == 0;
	}

	template<typename Mutex>
	class count_rotating_file_mt : public spdlog::sinks::base_sink<Mutex>
	{
	public:
		// base_filename: "logs/app.log" 或 "logs/app"
		// 实际输出：logs/app.log, logs/app.1.log, logs/app.2.log...
		// max_files: 备份数量（.1 ~ .max_files），0 表示不保留备份（只保留 stem.log）
		count_rotating_file_mt(const std::string& base_filename,
							   size_t max_count,
							   size_t max_files = 0,
							   bool rotate_on_open = false,
							   bool strict_count_on_open = false)
			: max_count_(max_count), max_files_(max_files), rotate_on_open_(rotate_on_open), strict_count_on_open_(strict_count_on_open)
		{
			fs::path p(base_filename);
			dir_ = p.has_parent_path() ? p.parent_path() : fs::path(".");

			std::string fname = p.filename().string();
			if (ends_with(fname, ".log"))
				stem_ = fname.substr(0, fname.size() - 4);
			else
				stem_ = fname;

			extension_ = ".log";

			// 懒创建：不打开文件，只确保目录存在
			std::error_code ec;
			fs::create_directories(dir_, ec);

			opened_ = false;
			log_count_ = 0;
			rotated_on_open_done_ = false;
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			// rotate_on_open：第一次真正写入前...
			if (!rotated_on_open_done_)
			{
				if (rotate_on_open_ && max_files_ > 0)
				{
					std::error_code ec;
					if (fs::exists(base_path_(), ec) && !ec)
						rotate_files_();
				}
				rotated_on_open_done_ = true;
			}

			// 写入前：如果再写一条就会超限，则先滚动
			if (log_count_ >= max_count_)
			{
				if (max_files_ > 0)
				{
					rotate_files_();
					ensure_opened_for_write_();
				}
				else
				{
					// max_files==0：禁用滚动，继续追加写（可选：log_count_ 不要再增长避免溢出）
					// 这里直接钉住 log_count_，防止 size_t 无限增长
					log_count_ = max_count_;
				}
			}

			// 确保打开当前写入文件（stem.log），并在 strict 模式下统计已有行数
			ensure_opened_for_write_();

			// 写入前：如果再写一条就会超限，则先滚动（保证写入后不会超过 max_count）
			if (log_count_ >= max_count_)
			{
				rotate_files_();
				ensure_opened_for_write_();
			}

			spdlog::memory_buf_t buf;
			this->formatter_->format(msg, buf);

			file_helper_.write(buf);

			++log_count_;

			// 写完后：如果达到上限，关闭并滚动（不立刻创建新 stem.log，下一条写入时再创建/打开 -> 懒创建）
			if (log_count_ >= max_count_)
			{
				rotate_files_();
			}
		}

		void flush_() override
		{
			file_helper_.flush();
		}

	private:
		// logs/app.log
		fs::path base_path_() const
		{
			return dir_ / fs::path(stem_ + extension_);
		}

		// logs/app.N.log
		fs::path rotated_path_(size_t index) const
		{
			return dir_ / fs::path(stem_ + "." + std::to_string(index) + extension_);
		}

		static size_t count_lines_in_file_(const fs::path& filename)
		{
			std::ifstream in(filename.string(), std::ios::in | std::ios::binary);
			if (!in.is_open()) return 0;

			constexpr size_t kBuf = 64 * 1024;
			char buf[kBuf];

			size_t lines = 0;
			bool has_any = false;
			char last = '\0';

			while (in)
			{
				in.read(buf, static_cast<std::streamsize>(kBuf));
				std::streamsize n = in.gcount();
				if (n <= 0) break;

				has_any = true;
				last = buf[n - 1];

				for (std::streamsize i = 0; i < n; ++i)
					if (buf[i] == '\n') ++lines;
			}

			if (has_any && last != '\n')
				++lines;

			return lines;
		}

		// strict 模式：打开 stem.log 前统计已有行数；如果已满，先滚动再开
		void adjust_for_strict_on_open_()
		{
			if (!strict_count_on_open_ || max_count_ == 0)
			{
				log_count_ = 0;
				return;
			}

			// 如果 stem.log 不存在 -> 0 行
			std::error_code ec;
			if (!fs::exists(base_path_(), ec) || ec)
			{
				log_count_ = 0;
				return;
			}

			size_t lines = count_lines_in_file_(base_path_());
			if (lines >= max_count_)
			{
				// stem.log 已满：先滚动（覆盖式），让新的 stem.log 从 0 开始
				rotate_files_();
				log_count_ = 0;
				return;
			}

			log_count_ = lines;
		}

		void ensure_opened_for_write_()
		{
			if (opened_)
				return;

			// strict：决定打开 stem.log 前要不要先滚动 & 初始化 log_count_
			adjust_for_strict_on_open_();

			const fs::path filename = base_path_();
			file_helper_.open(filename.string());
			opened_ = true;
		}

		// spdlog式覆盖滚动（rename 链条）
		// 关闭当前文件 -> 删除最老 -> N-1->N ... 1->2 -> base->1
		// 不打开新 base（懒创建，下一条写入才会打开）
		void rotate_files_()
		{
			file_helper_.close();
			opened_ = false;
			log_count_ = 0;

			if (max_files_ == 0)
			{
				return;
			}

			std::error_code ec;

			// 删除最老的 stem.max_files.log
			fs::remove(rotated_path_(max_files_), ec);

			// 搬运：stem.(i).log -> stem.(i+1).log (i = max_files-1..1)
			for (size_t i = max_files_ - 1; i >= 1; --i)
			{
				fs::path src = rotated_path_(i);
				fs::path dst = rotated_path_(i + 1);

				ec.clear();
				if (fs::exists(src, ec) && !ec)
				{
					// Windows 下 rename 目标存在会失败，先删
					fs::remove(dst, ec);
					ec.clear();
					fs::rename(src, dst, ec);
				}

				if (i == 1) break;// 防止 size_t 下溢
			}

			// base -> 1
			{
				fs::path src = base_path_();
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
		std::string stem_;
		std::string extension_;

		size_t max_count_;
		size_t max_files_;
		bool rotate_on_open_;
		bool strict_count_on_open_;

		size_t log_count_ = 0;
		bool opened_ = false;

		bool rotated_on_open_done_ = false;
	};

}// namespace CustomSink

#endif// COREXI_COMMON_PC_COUNT_ROTATING_SPDLOG_STYLE_SINK_HPP
