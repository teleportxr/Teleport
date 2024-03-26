#pragma once
#include <fstream>
#include <functional>
#include <filesystem>
namespace avs
{
	typedef uint64_t uid;
}
namespace teleport
{
	namespace core
	{
		class resource_ofstream : public std::ofstream
		{
		protected:
			std::function<std::string(avs::uid)> uid_to_path;

		public:
			std::string filename;
			resource_ofstream(const char *fn, std::function<std::string(avs::uid)> f)
				: std::ofstream(fn, std::ofstream::out | std::ofstream::binary), uid_to_path(f), filename(fn)
			{
				unsetf(std::ios_base::skipws);
			}
			template <typename T>
			void writeChunk(const T &t)
			{
				write((const char *)&t, sizeof(t));
			}
			friend resource_ofstream &operator<<(resource_ofstream &stream, avs::uid u)
			{
				if (!u)
				{
					size_t sz = 0;
					stream.write((char *)&sz, sizeof(sz));
				}
				else
				{
					std::string p = stream.uid_to_path(u);
					std::replace(p.begin(), p.end(), ' ', '%');
					std::replace(p.begin(), p.end(), '\\', '/');
					stream << p;
				}
				return stream;
			}
			friend resource_ofstream &operator<<(resource_ofstream &stream, const std::string &s)
			{
				size_t sz = s.length();
				stream.write((char *)&sz, sizeof(sz));
				stream.write(s.data(), s.length());
				return stream;
			}
		};
		class resource_ifstream : public std::ifstream
		{
		protected:
			std::function<avs::uid(std::string)> path_to_uid;
			size_t fileSize = 0;

		public:
			std::string filename;
			resource_ifstream(const char *fn, std::function<avs::uid(std::string)> f)
				: std::ifstream(fn, resource_ifstream::in | resource_ifstream::binary), path_to_uid(f), filename(fn)
			{
				fileSize = std::filesystem::file_size(fn);
				unsetf(std::ios_base::skipws);
			}
			template <typename T>
			void readChunk(T &t)
			{
				read((char *)&t, sizeof(t));
			}
			size_t getFileSize() const
			{
				return fileSize;
			}
			/// Get the number of bytes until the end of the file.
			size_t getBytesRemaining()
			{
				return fileSize - (size_t)tellg();
			}
			std::vector<char> readData()
			{
				std::vector<char> fileContents((std::istreambuf_iterator<char>(*this)),
											   std::istreambuf_iterator<char>());
				return fileContents;
			}
			friend resource_ifstream &operator>>(resource_ifstream &stream, avs::uid &u)
			{
				std::string p;
				stream >> p;
				u = stream.path_to_uid(p);
				return stream;
			}
			friend resource_ifstream &operator>>(resource_ifstream &stream, std::string &s)
			{
				size_t sz = 0;
				stream.read((char *)&sz, sizeof(sz));
				s.resize(sz);
				stream.read(s.data(), s.length());
				return stream;
			}
		};
	}
}