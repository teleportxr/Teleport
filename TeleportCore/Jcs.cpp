#include "TeleportCore/Jcs.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

using nlohmann::json;

namespace teleport
{
	namespace core
	{
		namespace
		{
			//! Decode UTF-8 to UTF-16 code units. Malformed input is passed
			//! through as U+FFFD rather than throwing: a key we cannot decode
			//! still has to sort somewhere, deterministically, and refusing to
			//! canonicalise a document because of one bad byte would be a
			//! worse failure than ordering it oddly.
			std::u16string Utf8ToUtf16(const std::string &s)
			{
				std::u16string out;
				out.reserve(s.size());
				size_t i = 0;
				while (i < s.size())
				{
					const unsigned char c = static_cast<unsigned char>(s[i]);
					uint32_t cp = 0;
					size_t extra = 0;
					if (c < 0x80)			{ cp = c;			extra = 0; }
					else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F;	extra = 1; }
					else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F;	extra = 2; }
					else if ((c & 0xF8) == 0xF0) { cp = c & 0x07;	extra = 3; }
					else					{ cp = 0xFFFD;		extra = 0; }

					if (i + extra >= s.size())
					{
						cp = 0xFFFD;
						extra = 0;
					}
					for (size_t k = 1; k <= extra; k++)
					{
						const unsigned char cc = static_cast<unsigned char>(s[i + k]);
						if ((cc & 0xC0) != 0x80)
						{
							cp = 0xFFFD;
							extra = 0;
							break;
						}
						cp = (cp << 6) | (cc & 0x3F);
					}
					i += extra + 1;

					if (cp > 0x10FFFF)
						cp = 0xFFFD;
					if (cp >= 0x10000)
					{
						cp -= 0x10000;
						out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
						out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
					}
					else
					{
						out.push_back(static_cast<char16_t>(cp));
					}
				}
				return out;
			}

			//! JSON string escaping, matching ECMAScript JSON.stringify: the
			//! short forms where they exist, \u00xx for the remaining control
			//! characters, and nothing else escaped — not the forward slash,
			//! not non-ASCII, which passes through as UTF-8.
			void AppendEscapedString(std::string &out, const std::string &s)
			{
				out.push_back('"');
				for (const char ch : s)
				{
					const unsigned char c = static_cast<unsigned char>(ch);
					switch (c)
					{
					case '"':	out += "\\\"";	break;
					case '\\':	out += "\\\\";	break;
					case '\b':	out += "\\b";	break;
					case '\f':	out += "\\f";	break;
					case '\n':	out += "\\n";	break;
					case '\r':	out += "\\r";	break;
					case '\t':	out += "\\t";	break;
					default:
						if (c < 0x20)
						{
							char buf[7];
							std::snprintf(buf, sizeof(buf), "\\u%04x", c);
							out += buf;
						}
						else
						{
							out.push_back(ch);
						}
						break;
					}
				}
				out.push_back('"');
			}

			//! Split a shortest-round-trip decimal rendering into the (s, n)
			//! pair ECMAScript Number::toString is defined in terms of:
			//! `digits` are the significant digits with no leading or trailing
			//! zeros, and `n` is the position of the decimal point relative to
			//! the front of them, i.e. value = 0.<digits> * 10^n.
			void SplitDecimal(const std::string &text, std::string &digits, int &n)
			{
				std::string mantissa = text;
				int exponent = 0;
				const size_t e = mantissa.find_first_of("eE");
				if (e != std::string::npos)
				{
					exponent = std::stoi(mantissa.substr(e + 1));
					mantissa = mantissa.substr(0, e);
				}

				std::string all;
				int pointPos = 0;
				const size_t dot = mantissa.find('.');
				if (dot == std::string::npos)
				{
					all = mantissa;
					pointPos = static_cast<int>(all.size());
				}
				else
				{
					all = mantissa.substr(0, dot) + mantissa.substr(dot + 1);
					pointPos = static_cast<int>(dot);
				}

				size_t lead = 0;
				while (lead < all.size() && all[lead] == '0')
					lead++;
				size_t end = all.size();
				while (end > lead && all[end - 1] == '0')
					end--;

				if (lead >= end)
				{
					digits = "0";
					n = 1;
					return;
				}

				digits = all.substr(lead, end - lead);
				n = pointPos - static_cast<int>(lead) + exponent;
			}
		}

		int CompareUtf16(const std::string &a, const std::string &b)
		{
			const std::u16string ua = Utf8ToUtf16(a);
			const std::u16string ub = Utf8ToUtf16(b);
			if (ua < ub) return -1;
			if (ub < ua) return 1;
			return 0;
		}

		//! ECMAScript Number::toString (ES2023 6.1.6.1.20), which is the
		//! serialisation RFC 8785 §3.2.2.3 points at. It is emphatically not
		//! printf: the choice between positional and exponential notation is
		//! made on the decimal exponent alone (exponential below 1e-6 and at
		//! or above 1e21), never on whichever happens to be shorter, and the
		//! exponent itself carries no padding zeros.
		std::string SerializeJsonNumber(double value)
		{
			if (!std::isfinite(value))
				throw std::invalid_argument("jcs: cannot canonicalise non-finite number");
			if (value == 0.0)
				return "0";	// also folds -0 to 0, as ECMAScript does

			std::string sign;
			if (value < 0)
			{
				sign = "-";
				value = -value;
			}

			// std::to_chars without a format argument produces the shortest
			// representation that round-trips, which is exactly the (s, k)
			// pair the spec asks for; only the presentation differs.
			char buf[64];
			const auto res = std::to_chars(buf, buf + sizeof(buf), value);
			if (res.ec != std::errc())
				throw std::invalid_argument("jcs: number could not be rendered");

			std::string digits;
			int n = 0;
			SplitDecimal(std::string(buf, res.ptr), digits, n);
			const int k = static_cast<int>(digits.size());

			std::string out;
			if (k <= n && n <= 21)
			{
				out = digits + std::string(static_cast<size_t>(n - k), '0');
			}
			else if (0 < n && n <= 21)
			{
				out = digits.substr(0, static_cast<size_t>(n)) + "." + digits.substr(static_cast<size_t>(n));
			}
			else if (-6 < n && n <= 0)
			{
				out = "0." + std::string(static_cast<size_t>(-n), '0') + digits;
			}
			else
			{
				const int e = n - 1;
				const std::string exponent = (e >= 0 ? "e+" : "e-") + std::to_string(e >= 0 ? e : -e);
				out = (k == 1) ? digits + exponent
							   : digits.substr(0, 1) + "." + digits.substr(1) + exponent;
			}
			return sign + out;
		}

		namespace
		{
			void Serialize(std::string &out, const json &value)
			{
				switch (value.type())
				{
				case json::value_t::null:
					out += "null";
					break;
				case json::value_t::boolean:
					out += value.get<bool>() ? "true" : "false";
					break;
				case json::value_t::number_integer:
					out += std::to_string(value.get<int64_t>());
					break;
				case json::value_t::number_unsigned:
					out += std::to_string(value.get<uint64_t>());
					break;
				case json::value_t::number_float:
					out += SerializeJsonNumber(value.get<double>());
					break;
				case json::value_t::string:
					AppendEscapedString(out, value.get_ref<const std::string &>());
					break;
				case json::value_t::array:
				{
					out.push_back('[');
					bool first = true;
					for (const auto &element : value)
					{
						if (!first)
							out.push_back(',');
						first = false;
						Serialize(out, element);
					}
					out.push_back(']');
					break;
				}
				case json::value_t::object:
				{
					// nlohmann orders its map byte-wise, which agrees with
					// UTF-16 order only below U+10000. Sort explicitly.
					std::vector<std::pair<const std::string *, const json *>> members;
					members.reserve(value.size());
					for (auto it = value.begin(); it != value.end(); ++it)
						members.emplace_back(&it.key(), &it.value());
					std::sort(members.begin(), members.end(),
						[](const std::pair<const std::string *, const json *> &a,
						   const std::pair<const std::string *, const json *> &b)
						{
							return CompareUtf16(*a.first, *b.first) < 0;
						});

					out.push_back('{');
					bool first = true;
					for (const auto &member : members)
					{
						if (!first)
							out.push_back(',');
						first = false;
						AppendEscapedString(out, *member.first);
						out.push_back(':');
						Serialize(out, *member.second);
					}
					out.push_back('}');
					break;
				}
				case json::value_t::binary:
				case json::value_t::discarded:
				default:
					throw std::invalid_argument("jcs: value has no JSON representation");
				}
			}
		}

		std::string CanonicalizeJson(const json &value)
		{
			std::string out;
			Serialize(out, value);
			return out;
		}
	}
}
